#include <chrono>

using namespace std::chrono;

extern int GetMilliseconds(void)
{
    static long long start_ms = 0;
    if (start_ms == 0)
    {
        start_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        return 0;
    }
    const long long now_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    return static_cast<int>(now_ms - start_ms);
}