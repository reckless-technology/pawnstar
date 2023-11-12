#include <chrono>

extern "C" int GetMilliseconds(void)
{
    static long long start_ms = 0;
    if (start_ms == 0)
    {
        start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        return 0;
    }
    const long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    return static_cast<int>(now_ms - start_ms);
}