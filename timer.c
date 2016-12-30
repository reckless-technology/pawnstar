#include "pawnstar.h"

#if _MSC_VER
#include <Windows.h>
/*
Get the system clock tick count in milliseconds (used for timing various 
performance measures)
*/
int GetMilliseconds(void)
{
    return GetTickCount();
}
#else
#include <time.h>
int GetMilliseconds(void)
{
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    return spec.tv_sec * 1000 + spec.tv_nsec / 1000000;
}
#endif