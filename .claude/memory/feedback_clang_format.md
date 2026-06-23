---
name: feedback_clang_format
description: Always run clang-format on edited C/C++ files before committing in pawnstar
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 4d178dc7-b2cc-4bef-90ca-ce2b53eb2817
---

After editing any C/C++ source/header in pawnstar, run **clang-format** on the touched files before
committing (the repo uses clang-format, Microsoft base style, 120-col; config in `.clang-format`). The
existing code is already formatted, so `clang-format -i <files>` only reformats the lines you changed.

**Why:** Jonny formats the code and expects committed diffs to be clang-format-clean; he had to reformat
chess_clock.h himself after I committed an unformatted refactor (2026-06-20).

**How to apply:** `clang-format -i src/<changed files>` (binary at `/usr/bin/clang-format`) as the last step
before `git add`/commit. Related: [[feedback_descriptive_names]], [[feedback_make_clean]].
