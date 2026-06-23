---
name: feedback_push_to_main
description: User permits committing and pushing directly to the main branch in pawnstar
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 9060cfe0-a261-495f-b0ec-b6db79ed2919
---

For the pawnstar repo, the user is fine with committing and pushing directly to the `main` branch — no need to create a feature branch first when asked to commit/push.

**Why:** The user works solo on this repo and merges their own PRs; they explicitly said "you can push directly to main branch."

**How to apply:** When the user asks to commit and push and the checkout is on `main`, just commit and push to `main`. Still only commit/push when explicitly asked.
