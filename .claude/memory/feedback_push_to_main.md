---
name: feedback_push_to_main
description: "STALE (withdrawn 2026-10-26): direct pushes to main were permitted; pawnstar now takes pull requests only"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 9060cfe0-a261-495f-b0ec-b6db79ed2919
---

**STALE (correction 2026-11-24): this permission was withdrawn on 2026-10-26.** Jonny set pawnstar to pull requests to `main`, enforced by a ruleset (pull request required, rebase only, `CI` green and up to date); direct pushes are refused. Do not act on the note below. The current policy is in `reckless-working-practice`, `references/repo-policies.md`. (`pawnstar-go` still allows direct pushes to `main`.)

For the pawnstar repo, the user is fine with committing and pushing directly to the `main` branch — no need to create a feature branch first when asked to commit/push.

**Why:** The user works solo on this repo and merges their own PRs; they explicitly said "you can push directly to main branch."

**How to apply:** When the user asks to commit and push and the checkout is on `main`, just commit and push to `main`. Still only commit/push when explicitly asked.
