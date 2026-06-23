---
name: feedback_descriptive_names
description: "Jonny prefers long, descriptive identifier names and dislikes abbreviations in code"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 4d178dc7-b2cc-4bef-90ca-ce2b53eb2817
---

Jonny prefers **long, descriptive names** for identifiers (variables, functions, types, members) and
**dislikes abbreviations**. When naming new code, spell words out.

**Why:** readability/clarity; he actively renames abbreviated identifiers in the pawnstar codebase to their
full forms.

**How to apply:** avoid abbreviations in new identifiers. Examples of his renames (2026-06-20):
`cont_hist_` → `continuation_history_`, `RecordContHist` → `RecordContinuationHistory`,
`ContHistScore` → `ContinuationHistScore`. So: expand `cont`→`continuation`, `hist`→`history`, etc.
Existing abbreviations I introduced that he may want expanded (offer, don't assume): `kIirMinDepth` (IIR =
internal iterative reduction), and the trailing-underscore member style is kept (`continuation_history_`).
When in doubt, match the fuller spelling and ask. Related: [[project_tried_search_features]].

**Boolean naming (2026-06-20):** boolean variables and functions that return `bool` should start with one of
`is` / `has` / `can` / `may` / `do` / `does`, to make the meaning of `true` clear. E.g. `IsLoaded()`,
`is_pondering`, `is_cancel_pending` (good); `NnueActive()` was bad and was deleted/replaced by
`Network::IsLoaded()`. Apply to all new bool identifiers.
