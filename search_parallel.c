#include "pawnstar.h"
#include <Windows.h>
/******************************************************************************
Functions to support parallel search. SearchTasks are added to a list of
pending tasks, and worker threads (one per processor in the system) consume 
pending tasks. A pending task may be aborted prior to execution, for example if
a beta cutoff renders it redundant.
*******************************************************************************/
static long                 num_running_tasks;
static HANDLE               search_complete_event;
static CONDITION_VARIABLE   new_task_cond_var[1];
static CRITICAL_SECTION     new_task_mutex[1];

static DWORD WINAPI WorkerThreadLoop(LPVOID args);
static void         StartWorkerThreads(int num_cpus);
/******************************************************************************
Initialize the worker threads and synchronization events.
*******************************************************************************/
void InitializeThreads(int num_cpus)
{
    InitializeConditionVariable(new_task_cond_var);
    InitializeCriticalSectionAndSpinCount(new_task_mutex, MUTEX_SPIN_COUNT);
    search_complete_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    StartWorkerThreads(num_cpus);
}
/******************************************************************************
Wait until all the workers have finished their work.
*******************************************************************************/
void WaitForSearchToComplete(void)
{
    WaitForSingleObject(search_complete_event, INFINITE);
}
/******************************************************************************
Obtain a new search task and return a pointer to it.
If move == 0 then the task will search all nodes, otherwise just search a 
single move.
*******************************************************************************/
SearchTask* NewSearchTask(const Position* src_position, 
                          int depth, 
                          int ply, 
                          int alpha, 
                          int beta, 
                          int move, 
                          int move_index)
{
    SearchTask* task   = ObtainTaskFromPool();
    task->src_position = src_position;
    task->depth        = depth;
    task->ply          = ply;
    task->alpha        = alpha;
    task->beta         = beta;
    task->move         = move;
    task->move_index   = move_index;
    task->cancel       = false;
    task->task_state   = TASK_PENDING;
    task->score        = ILLEGAL_SCORE;
    AddPendingTask(task);
    WakeConditionVariable(new_task_cond_var);
    return task;
}
/******************************************************************************
Called by a search thread whenever it needs to wait for another task to 
complete before it can return. 
Grab the next pending task and execute it.
*******************************************************************************/
void DoSomethingUseful(void)
{
    SearchTask* task;
    EnterCriticalSection(new_task_mutex);
    while ((task = GetNextPendingTask()) == NULL)
    {
        SleepConditionVariableCS(new_task_cond_var, new_task_mutex, INFINITE);
    }
    LeaveCriticalSection(new_task_mutex);
    if (_InterlockedCompareExchange(&task->task_state, TASK_RUNNING, TASK_PENDING) != TASK_PENDING)
    {
        /**********************************************************************
        Task was aborted prior to execution
        ***********************************************************************/
        ReturnTaskToPool(task);
        return;
    }
    _InterlockedIncrement(&num_running_tasks);
    if (task->move)
    {
        task->score = SearchSingleMove(
            task->src_position,
            task->depth, 
            task->ply, 
            task->alpha, 
            task->beta, 
            task->move, 
            task->move_index,
            false,
            &task->cancel);
    }
    else
    {
        task->score = Search(task->src_position, task->depth, task->ply, task->alpha, task->beta, &task->cancel);
    }
    _InterlockedExchange(&task->task_state, TASK_COMPLETED);
    if (_InterlockedDecrement(&num_running_tasks) == 0)
    {
        SetEvent(search_complete_event);
    }
}
/******************************************************************************
Abort a pending (or running) task. Typically invoked because a beta cutoff 
occurred during search which means that this task is now redundant.
*******************************************************************************/
void AbortTask(SearchTask* task)
{
    if (_InterlockedCompareExchange(&task->task_state, TASK_ABORTED, TASK_PENDING) == TASK_PENDING)
    {
        INCREMENT("task aborts pre-emptive");
        return;
    }
    task->cancel = true;
    while (task->task_state != TASK_COMPLETED)
    {
        DoSomethingUseful();
    }
    ReturnTaskToPool(task);
    INCREMENT("task aborts while running");
}
/******************************************************************************
Main search worker thread loop.
*******************************************************************************/
static DWORD WINAPI WorkerThreadLoop(LPVOID args)
{
    UNREFERENCED_PARAMETER(args);
    for ( ; ; )
    {
        DoSomethingUseful();
    }  
}
/******************************************************************************
Start the search worker threads.
*******************************************************************************/
static void StartWorkerThreads(int num_cpus)
{
    while (num_cpus--)
    {
        QueueUserWorkItem((LPTHREAD_START_ROUTINE)WorkerThreadLoop, NULL, WT_EXECUTEDEFAULT);
    }
}