#pragma once
/// @file platform.h Small OS/compiler-portability helpers.
///
/// Kept deliberately lightweight (no <windows.h>) so it can be included from the engine headers. The one
/// helper here is runtime CPU feature detection, which differs between the GCC/Clang `<cpuid.h>` builtins
/// and the MSVC/`<intrin.h>` `__cpuidex` intrinsic. Executable-path lookup (which needs <windows.h> on
/// Windows) lives in main.cpp, the only place that uses it, to keep <windows.h> out of the wider build.

#if defined(_WIN32)
#include <intrin.h>
#else
#include <cpuid.h>
#endif

namespace platform
{

/// @brief Whether the running CPU supports AVX-VNNI (CPUID.(EAX=7,ECX=1):EAX bit 4).
/// Used to pick the int8 output-dot kernel at runtime (see nnue::Network::Evaluate).
inline bool HasAvxVnni()
{
#if defined(_WIN32)
    int regs[4];
    __cpuidex(regs, 7, 1);
    return (static_cast<unsigned int>(regs[0]) & (1u << 4)) != 0u; // EAX bit 4
#else
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    if (__get_cpuid_count(7, 1, &eax, &ebx, &ecx, &edx) == 0)
    {
        return false;
    }
    return (eax & (1u << 4)) != 0u;
#endif
}

} // namespace platform
