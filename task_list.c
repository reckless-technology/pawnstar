#include "pawnstar.h"
#include <Windows.h>
/******************************************************************************
Functions to manage a list of pending search tasks and a list of free search 
tasks, which is used to avoid the overhead of frequent memory allocation; an 
expensive operation during search.

With the exception of one-time called FreeAllSearchTasks and InitializeTaskList,
all functions are thread-safe as they are called from each of the search worker 
threads.
*******************************************************************************/
static volatile int     pending_task_count;
static SearchTask*      pending_tasks_head;
static SearchTask*      pending_tasks_tail;
static SearchTask*      free_tasks;
static CRITICAL_SECTION pending_mutex[1];
static CRITICAL_SECTION free_mutex[1];
/******************************************************************************
Initialize the lists (set up the mutices)
*******************************************************************************/
void InitializeTaskList(void)
{
    InitializeCriticalSectionAndSpinCount(pending_mutex, MUTEX_SPIN_COUNT);
    InitializeCriticalSectionAndSpinCount(free_mutex,    MUTEX_SPIN_COUNT);
}
/******************************************************************************
Free all heap memory used by the lists
NOT THREAD SAFE
*******************************************************************************/
void FreeAllSearchTasks(void)
{
    SearchTask* task = free_tasks;
    SearchTask* next;
    while (task)
    {
        next = task->next;
        free(task);
        task = next;
    }    
    task = pending_tasks_head;
    while (task)
    {
        next = task->next;
        free(task);
        task = next;
    }
    free_tasks        = NULL;
    pending_tasks_head = NULL;
    pending_tasks_tail = NULL;
    pending_task_count = 0;
}
/******************************************************************************
Get a SearchTask from the free list, allocating extra tasks if required (this
should happen rarely in practice so is not a big performance hit)
*******************************************************************************/
SearchTask* ObtainTaskFromPool(void)
{
    SearchTask* result;
    EnterCriticalSection(free_mutex);
    result = free_tasks;
    if (!result)
    {
        int i;
        INCREMENT("task allocations");
        result = calloc(NUM_TASKS_TO_ALLOCATE_AT_ONCE, sizeof(SearchTask));
        for (i = 0; i != NUM_TASKS_TO_ALLOCATE_AT_ONCE - 1; ++i)
        {
            result[i].next = &result[i + 1];
        }
        result[NUM_TASKS_TO_ALLOCATE_AT_ONCE - 1].next = NULL;
    }
    free_tasks = result->next;
    MemoryBarrier();
    LeaveCriticalSection(free_mutex);
    return result;
}
/******************************************************************************
Return a SearchTask to the free list
*******************************************************************************/
void ReturnTaskToPool(SearchTask* task)
{
    EnterCriticalSection(free_mutex);
    task->next = free_tasks;
    free_tasks = task;
    MemoryBarrier();
    LeaveCriticalSection(free_mutex);
}
/******************************************************************************
Push a pending task to the head of the pending list
*******************************************************************************/
void AddPendingTask(SearchTask* task)
{
    EnterCriticalSection(pending_mutex);
    if (!pending_task_count)
    {
        task->prev = NULL;
        task->next = NULL;
        pending_tasks_head = task;
        pending_tasks_tail = task;
        pending_task_count = 1;
    }
    else
    {
        task->prev = NULL;
        task->next = pending_tasks_head;
        pending_tasks_head->prev = task;
        pending_tasks_head = task;
        ++pending_task_count;
    }
    MemoryBarrier();
    LeaveCriticalSection(pending_mutex);
}
/******************************************************************************
Pop a pending task from the tail of the pending list
*******************************************************************************/
SearchTask* GetNextPendingTask(void)
{
    SearchTask* result;
    EnterCriticalSection(pending_mutex);
    switch (pending_task_count)
    {
    case 0:
        result = NULL;
        break;
    case 1:
        result = pending_tasks_tail;
        pending_tasks_tail = NULL;
        pending_tasks_head = NULL;
        pending_task_count = 0;
        break;
    default:
        result = pending_tasks_tail;
        pending_tasks_tail = pending_tasks_tail->prev;
        pending_tasks_tail->next = NULL;
        --pending_task_count;
    }
    MemoryBarrier();
    LeaveCriticalSection(pending_mutex);
    return result;
}
/******************************************************************************
Returns the number of currently pending tasks
*******************************************************************************/
int GetNumberOfPendingTasks(void)
{
    return pending_task_count;
}