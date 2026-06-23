# `.claude/` — checked-in Claude Code project config

- `settings.json` — project permissions / settings.
- `memory/` — Claude Code's file-based project memories (the `MEMORY.md` index plus one file per
  memory). These are version-controlled here so they travel with the repo.

## How the memory sync works

Claude Code reads/writes project memories from a machine-local path derived from the project's absolute
working directory:

```
~/.claude/projects/<cwd-with-slashes-as-dashes>/memory
```

e.g. for `/home/jonny/work/pawnstar` that is
`~/.claude/projects/-home-jonny-work-pawnstar/memory`.

To keep the real files in the repo, that live directory is a **symlink** to `<repo>/.claude/memory`, so
every memory edit is an ordinary repo change you can commit and push.

## Setting it up on a new machine

```bash
git clone git@github.com:jonny-reckless/pawnstar.git
cd pawnstar
# encode the absolute repo path: replace each '/' with '-'
ENC="$(pwd | sed 's#/#-#g')"
LIVE="$HOME/.claude/projects/$ENC/memory"
mkdir -p "$(dirname "$LIVE")"
rm -rf "$LIVE"                      # remove any local memory dir first
ln -s "$(pwd)/.claude/memory" "$LIVE"
```

After that, Claude on the new machine sees all the checked-in memories and writes new ones straight into
the repo. (If you clone to a different path, just rerun the snippet — `ENC` recomputes from the new cwd.)
