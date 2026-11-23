# The reusable match workflow (`match-core.yml`)

Any reckless-technology chess engine can play a strength match on GitHub-hosted runners by building its two engines
and calling `.github/workflows/match-core.yml` from this repository. The caller builds (Go, Rust, C, C++ and .NET
builds differ); the core does everything that does not depend on the language: fastchess, the plan, the chunk
matrix, the pool. **The minutes are the calling repository's**, which keeps each engine's matches inside GitHub's
terms for testing the repository's own project. `.github/workflows/match.yml` is the first caller and the worked
example.

## What the core does

| job | what |
|---|---|
| `fastchess` | builds fastchess v1.8.2-alpha at commit `f618e34` with `build=release` (`-march=x86-64`; the default `-march=native` binary dies with SIGILL on another hosted CPU model), cached. Callers never build or pin it. |
| `plan` | unpacks the caller's artifact, verifies the opening set's SHA-256, strips `#` comments from it, and writes a matrix of **disjoint opening slices**, one position per colour-reversed pair, so no opening is replayed. A match that would run off the end of the set is refused. |
| `chunk` (matrix, `max-parallel: 40`) | runs the caller's optional `preflight`, otherwise a generic start-and-search probe of each engine, then `fastchess` on its slice; uploads its PGN, log and CPU model. |
| `pool` | `tools/opening_icc.py` (opening-corrected Elo with a 95% bar, recounts the games and **fails when the count is short**), `tools/pool_matches.py`, `tools/pgn_nps.py`, a per-chunk CPU-and-nps table, time forfeits; all in the job summary. |

Every chunk plays the flags `tools/pawnstar-match.sh` uses, so the two routes are one measurement: `-games 2
-repeat`, `order=sequential start=N`, `-draw movenumber=40 movecount=8 score=20 -resign movecount=3 score=600`,
Threads=1, a PGN with `nodes`, `seldepth` and `nps` in the move comments. `test/match_core_test.py` pins the command
to that list. A true sequential SPRT cannot be split across chunks: this route plays fixed-game chunks and tests the
pooled pentanomial at the end.

## Inputs

All strings.

| input | default | |
|---|---|---|
| `artifact` | required | name of the artifact the caller uploaded: `manifest.json`, the engines, their nets |
| `title` | `Match` | heading of the job summary |
| `tag` | `match` | prefix of the artifacts the core creates (`<tag>-fastchess`, `-plan`, `-chunk-N`, `-result`); give each call in one run its own |
| `time_control` | `8+0.08` | ignored when `depth` is set |
| `baseline_time_control` | empty | time odds: the baseline plays this clock instead |
| `depth` | empty | fixed depth instead of a clock |
| `games` | `4000` | rounded up to whole pairs |
| `chunks` | `20` | |
| `concurrency` | `2` | games at once per chunk: one per physical core (a hosted runner is 2 cores with SMT); a comma list is dealt out round-robin |
| `hash_mb` | `64` | |
| `first_opening` | `1` | 1-based; the chunks take consecutive slices from here |
| `openings_path` | empty | the opening set as a path **in the caller's repository** (checked out by the core); otherwise the manifest's `openings.path` inside the artifact |
| `openings_sha256` | empty | verified against the opening set; overrides the manifest's. One of the two must exist |
| `preflight` | empty | a shell command run in every chunk before playing; see below |

## The manifest (`manifest.json` at the root of the artifact)

```json
{
  "format": 1,
  "candidate": {
    "name": "Engine 1.4 (abc12345)",
    "command": "{inputs}/candidate/engine",
    "args": [],
    "options": {"EvalFile": "{inputs}/candidate.bin"},
    "env": {"ENGINE_EVALFILE": "{inputs}/candidate.bin"}
  },
  "baseline": { "name": "...", "command": "{inputs}/baseline/engine", "options": {}, "env": {} },
  "each_options": {"OwnBook": "false"},
  "openings": {"path": "openings.epd", "sha256": "<64 hex>"},
  "probe_expect": "regex the probe's output must match (optional)",
  "notes": ["lines shown in the job summary"]
}
```

* `candidate` and `baseline` are the two sides. fastchess always names them `candidate` and `baseline`, which the
  pool tools read; `name` is for the summary only.
* `command` and `args` start the engine. `{inputs}` anywhere in a command, an argument, an option value or an
  environment value is the absolute path of the unpacked artifact. A command may be a Go or Rust binary, a `dotnet`
  invocation (`"command": "dotnet", "args": ["{inputs}/candidate/Engine.dll"]`) or anything else that speaks UCI.
* `options` are that side's UCI options (`option.NAME=VALUE`), for example `EvalFile`. `env` is that side's
  environment, set by a generated wrapper script because fastchess has no flag for it (`PAWNSTAR_EVALFILE` in
  Pawnstar's case, which gives the engine its net at startup).
* `each_options` go to both sides. The core always adds `Threads=1` and `Hash=<hash_mb>`.
* Use distinct files for the two nets even if their contents agree.

Upload the artifact from the caller's build job with `actions/upload-artifact`; **executable bits are lost on
download**, and the core runs `chmod -R u+x` on the unpacked artifact.

## The probe and `preflight`

By default each chunk starts each engine the way fastchess will, sends its options, searches to depth 4 and requires
a `bestmove` (and, if the manifest has `probe_expect`, output matching that regex). That proves the engine starts and
searches; it cannot prove that a particular net loaded, which is the engine's to say. For that, give `preflight`: a
shell command (`bash -euo pipefail`) run in the unpacked artifact in every chunk, with `MATCH_INPUTS`,
`MATCH_TOOLS` (this repository's `tools/` at the called ref), `FASTCHESS`, `MATCH_CLOCK_ARGS`
(`--depth N` or `--time-control T`), `MATCH_START` and `MATCH_GAMES` set. A non-zero exit stops the chunk. When
`preflight` is given the generic probe is skipped: the command replaces it. Pawnstar's is its RNNUE1 header check
and depth-4 load probe, `"$MATCH_TOOLS/pawnstar-match.sh" ... --dry-run` (see `match.yml`).

## Pinning to a commit: how the tools get their ref

The pool tools (`tools/opening_icc.py`, `tools/pool_matches.py`, `tools/pgn_nps.py`) and the chunk runner
(`tools/match_core.py`) live in this repository, and the core checks them out **at the ref the workflow was called
at**, so a caller pinned to a commit runs that commit's workflow with that commit's tools. In a reusable workflow
`github.sha` and `github.workflow_sha` are the *caller's*, so they cannot be used. The `job` context carries the
called workflow's own `workflow_repository` and `workflow_sha`; the `plan` job refuses to run if either is empty,
checks out `tools/` from exactly that pair and passes it on as outputs, which the `chunk` and `pool` jobs check out
from, so all three use one commit. The pool's job summary prints the `repository@sha` it used.

## An example caller (another repository)

```yaml
name: Match
on:
  workflow_dispatch:
    inputs:
      games: { default: '4000' }
jobs:
  build:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v5
        with: { fetch-depth: 0 }
      - run: |
          # build the candidate and the baseline ... then
          mkdir -p inputs/candidate inputs/baseline
          cp build/engine inputs/candidate/engine          # the caller's own build, whatever the language
          cp baseline-build/engine inputs/baseline/engine
          cp nets/new.bin inputs/candidate.bin; cp nets/old.bin inputs/baseline.bin
          cp openings/book.epd inputs/openings.epd
          python3 - <<'PY'
          import hashlib, json
          side = lambda role: {"name": role, "command": "{inputs}/%s/engine" % role,
                               "options": {"EvalFile": "{inputs}/%s.bin" % role}}
          json.dump({"format": 1, "candidate": side("candidate"), "baseline": side("baseline"),
                     "each_options": {"OwnBook": "false"},
                     "openings": {"path": "openings.epd",
                                  "sha256": hashlib.sha256(open("inputs/openings.epd", "rb").read()).hexdigest()}},
                    open("inputs/manifest.json", "w"))
          PY
      - uses: actions/upload-artifact@v5
        with: { name: match-inputs, path: inputs, if-no-files-found: error }
  match:
    needs: build
    # Pin to a COMMIT, not a branch: the tools are checked out at that same commit.
    uses: reckless-technology/pawnstar/.github/workflows/match-core.yml@<commit sha>
    with:
      artifact: match-inputs
      title: 'Engine: candidate vs baseline'
      time_control: '8+0.08'
      games: ${{ inputs.games }}
```

A private caller may use this workflow because this repository is public. The caller's `permissions` need only
`contents: read`. A caller that calls the core twice in one run gives each call its own `tag`.

## Facts a caller should know

* A hosted runner is 4 vCPUs (2 cores with SMT), 16 GB, and **its CPU model is not fixed**, so nps varies up to 1.7x
  between chunks; a time-control match is fair within a chunk (both engines on one machine) and only approximately
  comparable with lin-0. Read the per-chunk table. A fixed-depth match is deterministic across machines.
* The openings: a set shorter than `first_opening + games/2 - 1` is refused. Duplicates *inside* a set are not
  detected (Pawnstar's `nnue/openings.epd` has some within its first 200 lines).
* `max-parallel` is 40 (the organisation's public repositories share about 60 hosted jobs).
* `-sprt` is not offered: a sequential test cannot be split across chunks. A serial SPRT stays a single job, capped
  at 6 hours.
