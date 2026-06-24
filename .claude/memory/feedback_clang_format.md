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

**How to apply:** `clang-format -i src/<changed files>` as the last step before `git add`/commit.

**VERSION MATTERS — pin 18.1.8.** The repo is clang-format-clean only under **clang-format 18.x**; newer
versions reformat committed code (tested 2026-06-23: v22 wants to touch chess_clock.h+nnue.h, v19/v20 touch
1 file, v17→3, v16→5; **only 18.1.8 = 0 files changed**). On the current machine clang-format is the pip
package `clang-format==18.1.8` installed in the venv `/home/jonny/work/.pawnstar`, symlinked to
`~/.local/bin/clang-format` (the old `/usr/bin/clang-format` is gone). Verify clean with
`clang-format --dry-run --Werror src/*.h src/*.cpp test/*.cpp`. See [[machine-setup-env]].
Related: [[feedback_descriptive_names]], [[feedback_make_clean]].
