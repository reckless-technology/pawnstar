#include "pawnstar.h"
#include <Windows.h>

/*
Get the system clock tick count in milliseconds (used for timing various 
performance measures)
*/
int GetMilliseconds(void)
{
    return GetTickCount();
}