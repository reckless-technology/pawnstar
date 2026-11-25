---
name: feedback-make-clean
description: Do not run make clean before every build; the Makefile is now a CMake wrapper that reconfigures itself. Clean only for a stale tree or a changed compiler
metadata:
  node_type: memory
  type: feedback
---

Run plain `make`. Use `make clean` only when a build looks stale (for example "no rule to make target" naming a file
that was renamed) or after switching `CXX`, which CMake caches.

**Why:** this note once said to always `make clean && make`, because stale `.d` files from an earlier Makefile build
named old paths after a rename. The `make` wrapper now configures with CMake on every invocation and picks up branch and
knob changes itself (CLAUDE.md, "Build"), so a blanket clean only throws away a good build tree and slows every loop.

**How to apply:** build with `make`; reach for `make clean` when the symptom above appears, then say so in the report.
