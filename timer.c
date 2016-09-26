#include "pawnstar.h"
#include <Windows.h>

static HANDLE           timer;
static volatile bool*   cancel_flag;
/*
Entry point for the background thread which sets the cancel thinking flag when
the maximum allocated thinking time for this move has elapsed
*/
static DWORD WINAPI TimerLoop(LPVOID arg)
{
    UNREFERENCED_PARAMETER(arg);
    for ( ; ; )
    {
        WaitForSingleObject(timer, INFINITE);
        if (cancel_flag)
        {
            *cancel_flag = true;
        }
    }
}
/*
Set the absolute maximum thinking time for next search
*/
void SetThinkingTimer(int milliseconds, volatile bool* cancel)
{
    LARGE_INTEGER timeout;
    cancel_flag = cancel;
    if (!timer)
    {
        timer = CreateWaitableTimer(NULL, FALSE, NULL);
        QueueUserWorkItem(TimerLoop, NULL, 0);
    }
    /*
    Time out units are multiples of 100ns, negative for relative timing
    */
    timeout.QuadPart = milliseconds * -10000LL;
    SetWaitableTimer(timer, &timeout, 0, NULL, NULL, FALSE);
}
/*
Cancel the thinking timer - typically if it was not required, we don't want it
going off in the middle of the thinking time for the next move!
*/
void CancelThinkingTimer(void)
{
    if (timer)
    {
        CancelWaitableTimer(timer);
    }
    cancel_flag = NULL;
}
/*
Get the system clock tick count in milliseconds (used for timing various 
performance measures)
*/
int GetMilliseconds(void)
{
    return GetTickCount();
}