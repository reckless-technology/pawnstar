/* Embeds the shipped NNUE net directly into the engine binary, so the engine still has a working
 * evaluator when the on-disk net cannot be loaded (wrong cwd, missing/renamed file). main.cpp loads the
 * file first and falls back to these bytes via Network::LoadFromMemory.
 *
 * The net to embed is EMBED_NET_PATH, defined on the assembler command line from the Makefile's EMBED_NET
 * (the single source of truth — no net filename is hard-coded here, so the embedded copy can never drift
 * from the shipped/Makefile net). This is a capital-.S file, so it is run through the C preprocessor.
 * The path is resolved relative to the build working directory (the repo root, where make runs). The image
 * is a stamped net (32-byte NetHeader + payload), so the same header validation applies. 64-byte aligned
 * for clean SIMD-friendly access of the payload.
 */
#ifndef EMBED_NET_PATH
#error "EMBED_NET_PATH must be defined by the build (see the Makefile EMBED_OBJECT rule)"
#endif
/* Read-only data section: ELF uses .rodata, COFF (Windows) uses .rdata. */
#if defined(__ELF__)
    .section .rodata
#else
    .section .rdata, "dr"
#endif
    .balign 64
    .global pawnstar_embedded_net
pawnstar_embedded_net:
    .incbin EMBED_NET_PATH
    .global pawnstar_embedded_net_end
pawnstar_embedded_net_end:

#if defined(__ELF__)
/* Mark the stack non-executable (data-only object); silences the linker's executable-stack warning.
 * ELF-only: COFF has no such note. */
    .section .note.GNU-stack,"",@progbits
#endif
