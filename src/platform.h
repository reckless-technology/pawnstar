#pragma once
/// @file platform.h Small OS/compiler-portability helpers.
///
/// Kept deliberately lightweight (no <windows.h>) so it can be included from the engine headers. The one
/// helper here is runtime CPU feature detection, which differs between the GCC/Clang `<cpuid.h>` builtins
/// and the MSVC/`<intrin.h>` `__cpuidex` intrinsic. Executable-path lookup (which needs <windows.h> on
/// Windows) lives in main.cpp, the only place that uses it, to keep <windows.h> out of the wider build.

// CPU-feature detection via cpuid is x86-only. <cpuid.h> / <intrin.h> exist (and the inline asm in them
// compiles) only on x86-64, so include them solely there; on other ISAs (e.g. arm64) there is no AVX-VNNI,
// and HasAvxVnni() below is a constant false — it is only ever consulted inside `#if defined(__AVX2__)`
// (see nnue::Network::Evaluate), so it is never actually called off x86.
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#define PAWNSTAR_X86 1
#if defined(_WIN32)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

namespace platform
{

/// @brief Whether the running CPU supports AVX-VNNI (CPUID.(EAX=7,ECX=1):EAX bit 4).
/// Used to pick the int8 output-dot kernel at runtime (see nnue::Network::Evaluate). Always false on
/// non-x86 architectures (no AVX-VNNI there, and the kernel selection is x86-AVX2-gated anyway).
inline bool HasAvxVnni()
{
#if !defined(PAWNSTAR_X86)
    return false;
#elif defined(_WIN32)
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
