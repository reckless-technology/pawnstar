#!/usr/bin/env bash
# Set up a Linux or macOS machine to build, test, debug and format Pawnstar: the toolchain, the tools and the editor.
# Each component is checked first and installed only when it is missing, so the script is safe to run again at any
# time. The per-user tools go into the user's own profile (~/.local/bin, and ~/.cargo for training); the system
# packages come from apt (Debian, Ubuntu, Mint: sudo asks for a password) or Homebrew (macOS). tools/setup-dev.ps1 is
# the same for Windows. VS Code's "setup environment" and "check environment" tasks run the one for the platform.
#
# What it sets up:
#   - clang (clang-18, CI's compiler, and the clang++ the Makefile calls), CMake, Ninja, make and git: apt's on Linux;
#     Xcode's command-line tools and Homebrew's cmake and ninja on macOS
#   - bash >= 4 for the UCI suite's coproc (macOS's own is 3.2: Homebrew's, which CMakeLists.txt looks for first)
#   - clang-format 22.1.8, the format gate's version, through uv (bootstrapped into ~/.local/bin when it is absent)
#   - the repo's git hooks (core.hooksPath .githooks) and the VS Code extensions .vscode/extensions.json recommends
# With --docs: Doxygen and Graphviz, for `make doc`. With --training: what NNUE training with the bullet trainer needs
# - rustup with the stable toolchain, a check for an NVIDIA GPU and the CUDA toolkit on Linux (not installed: the
# script says where to get it), and bullet cloned at the commit tools/setup_bullet.sh pins.
#
#   tools/setup-dev.sh                # set up
#   tools/setup-dev.sh --check        # report only; exit 1 if anything is missing
#   tools/setup-dev.sh --no-persist   # add nothing to the shell profile's PATH (a test run in a scratch HOME)
#   tools/setup-dev.sh --docs         # and Doxygen with Graphviz
#   tools/setup-dev.sh --training     # and the NNUE trainer (nnue/README.md)
set -euo pipefail
cd "$(dirname "$0")/.."

check=0
persist=1
docs=0
training=0
for arg in "$@"; do
    case "$arg" in
        --check) check=1 ;;
        --no-persist) persist=0 ;;
        --docs) docs=1 ;;
        --training) training=1 ;;
        -h | --help) sed -n '2,23p' "$0"; exit 0 ;;
        *) echo "unknown argument: $arg" >&2; exit 2 ;;
    esac
done

# ---------------------------------------------------------------------------------------------------------------------
# Helpers (the same in every reckless-technology repo's setup script).
missing=0
ok() { printf '  \033[32mok\033[0m       %-24s %s\n' "$1" "$2"; }
miss() { missing=$((missing + 1)); printf '  \033[33mmissing\033[0m  %-24s %s\n' "$1" "$2"; }
installing() { printf '  \033[36minstall\033[0m  %-24s %s\n' "$1" "$2"; }
note() { printf '  \033[35mnote\033[0m     %-24s %s\n' "$1" "$2"; }
have() { command -v "$1" >/dev/null 2>&1; }
os=$(uname -s)

# Put a directory on this shell's PATH and, unless --no-persist or --check, on every new shell's (the profiles a login
# shell, bash and zsh read).
add_to_path() {
    case ":$PATH:" in *":$1:"*) ;; *) PATH="$1:$PATH"; export PATH ;; esac
    [ "$persist" = 1 ] && [ "$check" = 0 ] || return 0
    local line="export PATH=\"$1:\$PATH\""
    local profiles=("$HOME/.profile" "$HOME/.bashrc")
    [ "$os" = Darwin ] && profiles+=("$HOME/.zprofile")
    for f in "${profiles[@]}"; do
        grep -qxF "$line" "$f" 2>/dev/null || printf '\n%s\n' "$line" >>"$f"
    done
}

sha256_of() { if have sha256sum; then sha256sum "$1" | cut -d' ' -f1; else shasum -a 256 "$1" | cut -d' ' -f1; fi; }

# Download a file and refuse it unless its SHA-256 is the one given.
fetch_verified() {
    curl -fsSL "$1" -o "$3"
    local got
    got=$(sha256_of "$3")
    if [ "$got" != "$2" ]; then rm -f "$3"; echo "SHA-256 mismatch for $1 (got $got, want $2)" >&2; exit 1; fi
}

sudo_cmd() { if [ "$(id -u)" = 0 ]; then "$@"; else sudo "$@"; fi; }

# apt packages that are not installed yet, installed together (Debian, Ubuntu, Mint).
apt_packages() {
    local need=()
    for p in "$@"; do
        dpkg-query -W -f='${Status}' "$p" 2>/dev/null | grep -q 'install ok installed' || need+=("$p")
    done
    if [ ${#need[@]} = 0 ]; then ok 'apt packages' "$*"; return; fi
    if [ "$check" = 1 ]; then miss 'apt packages' "${need[*]}"; return; fi
    installing 'apt packages' "${need[*]}"
    sudo_cmd apt-get update -q
    sudo_cmd env DEBIAN_FRONTEND=noninteractive apt-get install -y -q "${need[@]}"
}

# Homebrew formulae that are not installed yet.
brew_packages() {
    local need=()
    for p in "$@"; do brew list --formula "$p" >/dev/null 2>&1 || need+=("$p"); done
    if [ ${#need[@]} = 0 ]; then ok 'Homebrew packages' "$*"; return; fi
    if [ "$check" = 1 ]; then miss 'Homebrew packages' "${need[*]}"; return; fi
    installing 'Homebrew packages' "${need[*]}"
    brew install "${need[@]}"
}

# macOS: Xcode's command-line tools (the compilers, git) and Homebrew.
macos_base() {
    if xcode-select -p >/dev/null 2>&1; then ok 'Xcode command-line tools' "$(xcode-select -p)"
    elif [ "$check" = 1 ]; then miss 'Xcode command-line tools' 'xcode-select --install'
    else
        installing 'Xcode command-line tools' 'a dialog opens: install, then run this script again'
        xcode-select --install || true
        exit 0
    fi
    if ! have brew; then
        for b in /opt/homebrew/bin/brew /usr/local/bin/brew; do [ -x "$b" ] && eval "$("$b" shellenv)" && break; done
    fi
    if have brew; then ok 'Homebrew' "$(brew --version | head -1)"
    elif [ "$check" = 1 ]; then miss 'Homebrew' 'https://brew.sh'
    else
        installing 'Homebrew' 'the official installer (asks for your password)'
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        for b in /opt/homebrew/bin/brew /usr/local/bin/brew; do [ -x "$b" ] && eval "$("$b" shellenv)" && break; done
    fi
}

linux_base() {
    if have apt-get; then apt_packages "$@"
    else
        echo "  This is not a Debian-family Linux: install the equivalents of these packages, then run again: $*" >&2
        if [ "$check" = 1 ]; then missing=$((missing + 1)); fi
    fi
}

# uv, Astral's Python tool manager: on the PATH, or where its installer puts it (~/.local/bin). When it is absent, the
# release archive for this machine from Astral's GitHub releases, refused unless its SHA-256 is the one published beside
# it, unpacked into ~/.local/bin.
find_uv() {
    if have uv; then command -v uv
    elif [ -x "$HOME/.local/bin/uv" ]; then add_to_path "$HOME/.local/bin"; echo "$HOME/.local/bin/uv"
    fi
}

install_uv() {
    local triple
    case "$os-$(uname -m)" in
        Linux-x86_64) triple=x86_64-unknown-linux-gnu ;;
        Linux-aarch64) triple=aarch64-unknown-linux-gnu ;;
        Darwin-arm64) triple=aarch64-apple-darwin ;;
        Darwin-x86_64) triple=x86_64-apple-darwin ;;
        *) echo "no uv release for $os $(uname -m)" >&2; exit 1 ;;
    esac
    installing 'uv' "uv-$triple from Astral's GitHub releases" >&2
    local scratch url
    scratch=$(mktemp -d)
    url="https://github.com/astral-sh/uv/releases/latest/download/uv-$triple.tar.gz"
    fetch_verified "$url" "$(curl -fsSL "$url.sha256" | cut -d' ' -f1)" "$scratch/uv.tar.gz"
    tar -xzf "$scratch/uv.tar.gz" -C "$scratch"
    mkdir -p "$HOME/.local/bin"
    cp "$scratch/uv-$triple/uv" "$scratch/uv-$triple/uvx" "$HOME/.local/bin/"
    rm -rf "$scratch"
    add_to_path "$HOME/.local/bin"
    echo "$HOME/.local/bin/uv"
}

# The version an executable's --version reports ("clang-format version 22.1.8 (...)"), or nothing.
# sed reads every line, so a longer report never meets a closed pipe under pipefail.
tool_version() { "$1" --version 2>/dev/null | sed -n '1s/.*version \([0-9][0-9.]*\).*/\1/p'; }

set_git_hooks() {
    [ -d .githooks ] || return 0
    if [ "$(git config core.hooksPath || true)" = .githooks ]; then ok 'git hooks' 'core.hooksPath .githooks'
    elif [ "$check" = 1 ]; then miss 'git hooks' 'core.hooksPath is not .githooks'
    else git config core.hooksPath .githooks; installing 'git hooks' 'core.hooksPath .githooks'; fi
}

# The extensions .vscode/extensions.json recommends, when VS Code's command line is on the PATH.
install_vscode_extensions() {
    have code && [ -f .vscode/extensions.json ] || return 0
    local installed
    installed=$(code --list-extensions 2>/dev/null || true)
    for ext in $(sed -e 's://.*$::' .vscode/extensions.json | tr -d '\n' | sed -e 's/.*"recommendations"[^[]*\[//' -e 's/\].*//' | tr -d '" ' | tr ',' ' '); do
        if printf '%s\n' "$installed" | grep -qixF "$ext"; then ok 'VS Code extension' "$ext"
        elif [ "$check" = 1 ]; then miss 'VS Code extension' "$ext"
        else installing 'VS Code extension' "$ext"; code --install-extension "$ext" --force >/dev/null; fi
    done
}

# ---------------------------------------------------------------------------------------------------------------------
echo "pawnstar: $([ "$check" = 1 ] && echo checking || echo 'setting up') the $os development environment"

# The compiler, the build tools and git. CI builds with apt's clang-18 (.github/workflows/ci.yml); the clang package
# is the clang++ the Makefile calls by default (clang-18 itself on Ubuntu 24.04 and Mint 22).
doc_packages=()
case "$os" in
    Linux)
        [ "$docs" = 1 ] && doc_packages=(doxygen graphviz)
        linux_base clang clang-18 cmake ninja-build make git curl ${doc_packages[@]+"${doc_packages[@]}"} ;;
    Darwin)
        [ "$docs" = 1 ] && doc_packages=(doxygen graphviz)
        macos_base
        if have brew; then brew_packages cmake ninja bash ${doc_packages[@]+"${doc_packages[@]}"}; fi ;;
    *) echo "unsupported system: $os (tools/setup-dev.ps1 is the Windows script)" >&2; exit 2 ;;
esac
if have clang++; then ok 'clang++' "$(clang++ --version | head -1)"; else miss 'clang++' 'the compiler the Makefile calls'; fi

# bash >= 4, which the UCI suite's coproc needs.
bash4=""
for b in /opt/homebrew/bin/bash /usr/local/bin/bash /bin/bash /usr/bin/bash; do
    if [ -x "$b" ] && "$b" -c '[[ ${BASH_VERSINFO[0]} -ge 4 ]]' 2>/dev/null; then bash4=$b; break; fi
done
if [ -n "$bash4" ]; then ok 'bash >= 4' "$bash4 ($("$bash4" -c 'echo $BASH_VERSION'))"; else miss 'bash >= 4' 'for the UCI suite'; fi

# clang-format 22.1.8, the version CI's format job installs (.github/workflows/ci.yml): any first on the PATH that
# reports it, else the PyPI wheel as a uv tool.
format_version=22.1.8
first_format=$(command -v clang-format || true)
first_version=""
[ -n "$first_format" ] && first_version=$(tool_version "$first_format")
if [ "$first_version" = "$format_version" ]; then ok 'clang-format' "$format_version ($first_format)"
else
    uv=$(find_uv || true)
    tool_bin="$HOME/.local/bin"
    if [ -n "$uv" ]; then tool_bin=$("$uv" tool dir --bin 2>/dev/null || echo "$tool_bin"); fi
    uv_format="$tool_bin/clang-format"
    if ! { [ -x "$uv_format" ] && [ "$(tool_version "$uv_format")" = "$format_version" ]; }; then
        if [ "$check" = 1 ]; then
            miss 'clang-format' "$format_version (uv tool install clang-format==$format_version)"
            uv_format=""
        else
            [ -n "$uv" ] || uv=$(install_uv)
            installing 'clang-format' "$format_version, uv tool install clang-format==$format_version"
            "$uv" tool install "clang-format==$format_version"
            tool_bin=$("$uv" tool dir --bin)
            uv_format="$tool_bin/clang-format"
        fi
    fi
    if [ -n "$uv_format" ]; then
        add_to_path "$tool_bin"
        ok 'clang-format' "$(tool_version "$uv_format") ($uv_format)"
        if [ -n "$first_format" ] && [ "$first_format" != "$uv_format" ]; then
            note 'clang-format' "$first_format ($first_version) shadows it wherever $tool_bin is not first on PATH: remove it"
        fi
    fi
fi

# Doxygen and Graphviz's dot, for `make doc` (the packages came with the system's above).
if [ "$docs" = 1 ]; then
    if have doxygen; then ok 'Doxygen' "$(doxygen --version | head -1)"; else miss 'Doxygen' 'for make doc'; fi
    if have dot; then ok 'Graphviz' "$(dot -V 2>&1 | head -1)"; else miss 'Graphviz' 'for make doc'; fi
fi

# NNUE training (nnue/README.md): the bullet trainer is Rust, built with cargo, on CUDA when there is an NVIDIA GPU.
if [ "$training" = 1 ]; then
    cargo_bin="${CARGO_HOME:-$HOME/.cargo}/bin"
    if [ -x "$cargo_bin/rustup" ]; then add_to_path "$cargo_bin"; fi
    if have rustup; then ok 'rustup' "$(rustup --version 2>/dev/null | head -1)"
    elif [ "$check" = 1 ]; then miss 'rustup' 'and the stable toolchain, for bullet'
    else
        case "$os-$(uname -m)" in
            Linux-x86_64) triple=x86_64-unknown-linux-gnu ;;
            Linux-aarch64) triple=aarch64-unknown-linux-gnu ;;
            Darwin-arm64) triple=aarch64-apple-darwin ;;
            Darwin-x86_64) triple=x86_64-apple-darwin ;;
            *) echo "no rustup-init for $os $(uname -m)" >&2; exit 1 ;;
        esac
        installing 'rustup' "rustup-init ($triple) from static.rust-lang.org"
        scratch=$(mktemp -d)
        url="https://static.rust-lang.org/rustup/dist/$triple/rustup-init"
        fetch_verified "$url" "$(curl -fsSL "$url.sha256" | cut -d' ' -f1)" "$scratch/rustup-init"
        chmod +x "$scratch/rustup-init"
        "$scratch/rustup-init" -y --default-toolchain none --no-modify-path >/dev/null
        rm -rf "$scratch"
        add_to_path "$cargo_bin"
    fi
    if have rustup; then
        # bullet's manifest is edition 2024, which needs a recent cargo: rustup's stable, never apt's cargo.
        if rustup toolchain list 2>/dev/null | grep -q '^stable'; then ok 'Rust stable' "$(rustc +stable --version)"
        elif [ "$check" = 1 ]; then miss 'Rust stable' 'rustup toolchain install stable'
        else
            installing 'Rust stable' 'rustup toolchain install stable --profile minimal'
            rustup toolchain install stable --profile minimal
        fi
    fi

    # CUDA is checked, never installed: the toolkit is large, wants an administrator and must suit the driver.
    gpu=""
    if have nvidia-smi; then gpu=$(nvidia-smi --query-gpu=name,driver_version --format=csv,noheader 2>/dev/null | head -1 || true); fi
    if [ -z "$gpu" ]; then note 'NVIDIA GPU' 'none found: bullet trains on the CPU with BULLET_FEATURES= (slowly)'
    else
        ok 'NVIDIA GPU' "$gpu"
        cuda="${CUDA_PATH:-/usr/local/cuda}"
        if [ -x "$cuda/bin/nvcc" ]; then ok 'CUDA toolkit' "$("$cuda/bin/nvcc" --version | sed -n 's/.*release \([0-9.]*\),.*/\1/p') ($cuda)"
        else miss 'CUDA toolkit' "nvcc in ${cuda}: install it from https://developer.nvidia.com/cuda-downloads (12.3 or later)"; fi
    fi

    # bullet at the commit tools/setup_bullet.sh pins, with Pawnstar's examples installed - that script does it, so
    # there is one pin and one recipe.
    pin=$(sed -n 's/^BULLET_COMMIT=\([0-9a-f]*\).*/\1/p' tools/setup_bullet.sh)
    bullet="${BULLET:-$HOME/pawnstar_nnue/bullet}"
    head=""
    if [ -d "$bullet/.git" ]; then head=$(git -C "$bullet" rev-parse HEAD 2>/dev/null || true); fi
    if [ "$head" = "$pin" ] && [ -f "$bullet/examples/pawnstar.rs" ] && [ -f "$bullet/examples/pawnstar_eval.rs" ]; then
        ok 'bullet' "$bullet at ${pin:0:12}"
    elif [ "$check" = 1 ]; then miss 'bullet' "$bullet at ${pin:0:12} (tools/setup_bullet.sh)"
    else
        installing 'bullet' "$bullet at ${pin:0:12}, through tools/setup_bullet.sh"
        BULLET="$bullet" bash tools/setup_bullet.sh
    fi
fi

set_git_hooks
install_vscode_extensions

if [ "$check" = 1 ]; then
    if [ "$missing" -gt 0 ]; then echo "$missing missing: run tools/setup-dev.sh (VS Code: the 'setup environment' task)."; exit 1; fi
    echo 'Everything is in place.'
else
    echo 'Done. Open a new terminal (or restart VS Code) for PATH changes to reach it, then: make check'
fi
