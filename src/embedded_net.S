/* Embeds the shipped NNUE net (nnue/pawnstar-v8.bin) directly into the engine binary, so the engine
 * still has a working evaluator when the on-disk net cannot be loaded (wrong cwd, missing/renamed file).
 * main.cpp loads the file first and falls back to these bytes via Network::LoadFromMemory.
 *
 * The .incbin path is resolved relative to the build working directory (the repo root, where make runs).
 * The image is byte-identical to nnue/pawnstar-v8.bin (a stamped net: 32-byte NetHeader + payload), so
 * the same header validation applies. 64-byte aligned for clean SIMD-friendly access of the payload.
 */
    .section .rodata
    .balign 64
    .global pawnstar_embedded_net
pawnstar_embedded_net:
    .incbin "nnue/pawnstar-v8.bin"
    .global pawnstar_embedded_net_end
pawnstar_embedded_net_end:

/* Mark the stack non-executable (data-only object); silences the linker's executable-stack warning. */
    .section .note.GNU-stack,"",@progbits
