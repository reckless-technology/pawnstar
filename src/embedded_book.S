/* Embeds the shipped opening book directly into the engine binary, so the engine still has a book when the
 * on-disk pawnstar.book cannot be loaded (wrong cwd, missing/renamed file). main.cpp loads the file first
 * and falls back to these bytes via OpeningBook::InitializeFromMemory.
 *
 * The book to embed is EMBED_BOOK_PATH, defined on the assembler command line from the Makefile's EMBED_BOOK
 * (the single source of truth — no book filename is hard-coded here, so the embedded copy can never drift
 * from the shipped/Makefile book). This is a capital-.S file, so it is run through the C preprocessor. The
 * path is resolved relative to the build working directory (the repo root, where make runs). The image is the
 * raw book text (not null-terminated); the byte count is the difference of the two symbols below.
 */
#ifndef EMBED_BOOK_PATH
#error "EMBED_BOOK_PATH must be defined by the build (see the Makefile EMBED_BOOK_OBJECT rule)"
#endif
    .section .rodata
    .balign 1
    .global pawnstar_embedded_book
pawnstar_embedded_book:
    .incbin EMBED_BOOK_PATH
    .global pawnstar_embedded_book_end
pawnstar_embedded_book_end:

/* Mark the stack non-executable (data-only object); silences the linker's executable-stack warning. */
    .section .note.GNU-stack,"",@progbits
