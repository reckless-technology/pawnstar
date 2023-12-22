#include <chrono>

using namespace std::chrono;

static system_clock::time_point start_time;
static bool has_been_called;

int GetMilliseconds()
{
    if (!has_been_called)
    {
        has_been_called = true;
        start_time = system_clock::now();
        return 0;
    }
    const auto now_time = system_clock::now();
    return duration_cast<milliseconds>(now_time - start_time).count();
}