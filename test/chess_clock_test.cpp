/// @file chess_clock_test.cpp Unit tests for the std::chrono ChessClock timing logic.
///
/// Exercises the parts `make check`'s fixed-depth suites don't: arming/checking the hard deadline, elapsed
/// time since search start, the ponderhit budget conversion, the "no hard deadline" sentinel, and Reset().
/// Uses real (short) sleeps on the steady clock. Bounds are deliberately generous on the "must have elapsed"
/// side so the test stays reliable under CPU load (a sleep may overrun, never underrun), and the
/// "no deadline" checks can never spuriously trip.

#include "chess_clock.h"

#include <chrono>
#include <cstdio>
#include <thread>

using namespace std::chrono_literals;
using Duration = ChessClock::Duration;

static int failures = 0;

static void check(bool condition, const char *message)
{
    std::printf("%s: %s\n", condition ? "ok  " : "FAIL", message);
    if (!condition)
    {
        ++failures;
    }
}

static void sleep(Duration d)
{
    std::this_thread::sleep_for(d);
}

int main()
{
    // 1. StartSearch arms a hard deadline that fires once the maximum budget elapses (not before).
    {
        ChessClock clock;
        clock.StartSearch(/*allocated=*/100ms, /*maximum=*/200ms);
        check(!clock.HasReachedHardDeadline(), "deadline not reached immediately after StartSearch");
        check(clock.allocated_time_ == 100ms, "AllocatedTime returns the soft budget");
        sleep(120ms);
        check(!clock.HasReachedHardDeadline(), "deadline not reached before the maximum elapses");
        sleep(180ms); // total ~300ms > 200ms maximum
        check(clock.HasReachedHardDeadline(), "deadline reached after the maximum elapses");
    }

    // 2. A non-positive maximum means "no hard deadline" (fixed-depth / infinite): never reached.
    {
        ChessClock clock;
        clock.StartSearch(/*allocated=*/0ms, /*maximum=*/0ms);
        sleep(60ms);
        check(!clock.HasReachedHardDeadline(), "zero maximum => no hard deadline (fixed-depth / infinite)");
    }

    // 3. Elapsed time grows from the search start (lower bound only — robust under load).
    {
        ChessClock clock;
        clock.StartSearch(/*allocated=*/1000ms, /*maximum=*/1000ms);
        sleep(80ms);
        check(clock.ElapsedSinceSearchStart() >= 70ms, "elapsed >= time slept since StartSearch");
    }

    // 4. A ponder search has NO hard deadline until ponderhit; ponderhit then arms it from that moment and
    //    resets the elapsed origin (budget-from-ponderhit).
    {
        ChessClock clock;
        clock.StartPonderSearch(/*allocated=*/100ms, /*maximum=*/200ms);
        sleep(260ms); // longer than the maximum: a real deadline would have fired
        check(!clock.HasReachedHardDeadline(), "ponder search has no deadline before ponderhit");
        check(clock.ElapsedSinceSearchStart() >= 250ms, "ponder elapsed accumulates before ponderhit");

        clock.OnPonderhit();
        check(!clock.HasReachedHardDeadline(), "no deadline immediately after ponderhit");
        check(clock.ElapsedSinceSearchStart() < 100ms, "ponderhit resets the elapsed origin");
        sleep(280ms); // > 200ms maximum, measured from ponderhit
        check(clock.HasReachedHardDeadline(), "deadline reached after the post-ponderhit budget elapses");
    }

    // 5. Reset clears any armed deadline and restores the default time control.
    {
        ChessClock clock;
        clock.StartSearch(/*allocated=*/10ms, /*maximum=*/10ms);
        sleep(40ms);
        check(clock.HasReachedHardDeadline(), "deadline reached before Reset");
        clock.Reset();
        check(!clock.HasReachedHardDeadline(), "Reset clears the hard deadline");
        check(clock.clock_type_ == ChessClock::kStandard, "Reset restores kStandard clock type");
        check(clock.remaining_time_ == std::chrono::minutes{5}, "Reset restores the default remaining time");
        check(clock.moves_to_go_ == 0 && clock.depth_ == 10, "Reset restores default moves_to_go / depth");
    }

    if (failures == 0)
    {
        std::printf("CHESS CLOCK TEST PASS\n");
        return 0;
    }
    std::printf("CHESS CLOCK TEST FAIL (%d)\n", failures);
    return 1;
}
