<#
.SYNOPSIS
  Set up a Windows machine to build, test, debug and format Pawnstar: the toolchain, the tools and the editor.

.DESCRIPTION
  Each component is checked first and installed only when it is missing or not the version the repo wants, so the
  script is safe to run again at any time. Everything goes into the user's own profile and needs no administrator,
  except Visual Studio's components, which winget or the Visual Studio installer installs behind a UAC prompt.
  tools/setup-dev.sh is the same for Linux and macOS. VS Code's "setup environment" and "check environment" tasks run
  the one for the platform.

  What it sets up:
    - Git for Windows, whose bash runs the UCI suite (test/uci_test.sh) under ctest
    - Visual Studio (any edition, the Build Tools included) with the x64 C++ tools, a Windows SDK, CMake and Ninja:
      the instance tools/build.ps1 picks, the latest with the C++ tools
    - clang, which the engine is built with: that instance's 'C++ Clang tools for Windows' component, or a standalone
      LLVM for the MSVC ABI (C:\Program Files\LLVM, or clang++ on the PATH) in its place, as tools/build.ps1 accepts;
      the component is added only when there is neither, and the report says which clang the build will use
    - clang-format 22.1.8, the format gate's version, through uv (bootstrapped into ~\.local\bin when it is absent)
    - the repo's git hooks (core.hooksPath .githooks) and the VS Code extensions .vscode/extensions.json recommends
  With -Docs: Doxygen 1.14.0 in %LOCALAPPDATA%\Programs\doxygen-1.14.0 and Graphviz in %LOCALAPPDATA%\Programs\graphviz,
  for the doc target, each from its project's zip at a pinned SHA-256. With -Training: what NNUE training with the bullet trainer
  needs - rustup with the stable toolchain, a check for an NVIDIA GPU and the CUDA toolkit (not installed: the
  script says where to get it), and bullet cloned at the commit tools/setup_bullet.sh pins.

.PARAMETER Check
  Report what is present and what is missing, install nothing, and exit 1 if anything is missing.

.PARAMETER NoPersist
  Do not add anything to the user's PATH for good (only to this process's): for a test run in a scratch profile.

.PARAMETER Docs
  Also set up Doxygen and Graphviz, for tools\build.ps1 doc, in the user's profile (added to the user's PATH unless
  -NoPersist).

.PARAMETER Training
  Also set up NNUE training: Rust, the CUDA check and the bullet trainer (nnue/README.md).

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\setup-dev.ps1             # set up
  powershell -ExecutionPolicy Bypass -File tools\setup-dev.ps1 -Check      # report only
  powershell -ExecutionPolicy Bypass -File tools\setup-dev.ps1 -Training   # and the NNUE trainer
#>
param([switch]$Check, [switch]$NoPersist, [switch]$Docs, [switch]$Training)
# Continue, not Stop: Windows PowerShell 5.1 makes a native command's stderr a terminating error under Stop, even
# redirected (git and uv print their progress there). The cmdlets that must stop say so; native commands' exit codes
# are checked.
$ErrorActionPreference = 'Continue'
$ProgressPreference = 'SilentlyContinue'
Set-Location (Split-Path -Parent $PSScriptRoot)
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# ---------------------------------------------------------------------------------------------------------------------
# Helpers (the same in every reckless-technology repo's setup script).
$script:missing = 0
function Write-Ok([string]$name, [string]$detail) { Write-Host ("  ok       {0,-24} {1}" -f $name, $detail) -ForegroundColor Green }
function Write-Missing([string]$name, [string]$detail) {
  $script:missing++
  Write-Host ("  missing  {0,-24} {1}" -f $name, $detail) -ForegroundColor Yellow
}
function Write-Installing([string]$name, [string]$detail) { Write-Host ("  install  {0,-24} {1}" -f $name, $detail) -ForegroundColor Cyan }
function Write-Note([string]$name, [string]$detail) { Write-Host ("  note     {0,-24} {1}" -f $name, $detail) -ForegroundColor Magenta }

# Put a directory on this process's PATH and, unless -NoPersist or -Check, on the user's PATH for every new shell.
function Add-ToPath([string]$dir) {
  if (($env:PATH -split ';') -notcontains $dir) { $env:PATH = "$dir;$env:PATH" }
  if ($NoPersist -or $Check) { return }
  $user = [Environment]::GetEnvironmentVariable('Path', 'User')
  if ((($user -split ';') | Where-Object { $_ }) -notcontains $dir) {
    [Environment]::SetEnvironmentVariable('Path', (@($dir) + (($user -split ';') | Where-Object { $_ })) -join ';', 'User')
  }
}

# Download a file and refuse it unless its SHA-256 is the one given.
function Get-VerifiedFile([string]$url, [string]$sha256, [string]$out) {
  Invoke-WebRequest -Uri $url -OutFile $out -UseBasicParsing -ErrorAction Stop
  $got = (Get-FileHash $out -Algorithm SHA256).Hash
  if ($got -ne $sha256.ToUpper()) { Remove-Item $out; throw "SHA-256 mismatch for $url (got $got, want $sha256)" }
}

# A downloaded checksum file's text, whatever type Invoke-WebRequest hands back.
function Get-Text([string]$url) {
  $content = (Invoke-WebRequest -Uri $url -UseBasicParsing -ErrorAction Stop).Content
  if ($content -is [byte[]]) { return [Text.Encoding]::ASCII.GetString($content) }
  return $content
}

function New-ScratchDir {
  $dir = Join-Path ([IO.Path]::GetTempPath()) ("setup-dev-" + [Guid]::NewGuid().ToString('N').Substring(0, 8))
  New-Item -ItemType Directory -Path $dir -ErrorAction Stop | Out-Null
  return $dir
}

# Visual Studio (any edition, the Build Tools included) with the components given, found by vswhere.
function Find-VisualStudio([string[]]$components) {
  $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
  if (-not (Test-Path $vswhere)) { return $null }
  # Each component once: vswhere matches no instance at all when one is named twice.
  $found = & $vswhere -latest -products * -requires ($components | Select-Object -Unique) -property installationPath
  if ($found) { return ($found | Select-Object -First 1) }
  return $null
}

# Install Visual Studio Build Tools 2022 with the C++ workload, or add the components to the Visual Studio already
# there. Both need an administrator: winget and the Visual Studio installer raise the UAC prompt themselves.
function Install-VisualStudio([string[]]$components) {
  $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
  $existing = if (Test-Path $vswhere) { & $vswhere -latest -products * -property installationPath | Select-Object -First 1 }
  if ($existing) {
    Write-Installing 'Visual Studio' "adding the C++ components to $existing (UAC prompt)"
    $setup = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\setup.exe'
    # Components alone: the Build Tools' C++ workload id means nothing to Community or Professional.
    $add = $components | ForEach-Object { "--add $_" }
    $p = Start-Process -FilePath $setup -ArgumentList (@('modify', '--installPath', "`"$existing`"", '--passive', '--norestart', '--includeRecommended') + $add) -Wait -PassThru
  }
  else {
    Write-Installing 'Visual Studio' 'Build Tools 2022 with the C++ workload, through winget (UAC prompt)'
    $add = @('Microsoft.VisualStudio.Workload.VCTools') + $components | ForEach-Object { "--add $_" }
    $override = "--wait --passive --norestart --includeRecommended " + ($add -join ' ')
    $p = Start-Process -FilePath winget -ArgumentList @('install', '--id', 'Microsoft.VisualStudio.2022.BuildTools', '-e',
      '--accept-package-agreements', '--accept-source-agreements', '--override', "`"$override`"") -Wait -PassThru -NoNewWindow
  }
  if ($p.ExitCode -notin 0, 3010) { throw "the Visual Studio installer exited with $($p.ExitCode)" }
}

# uv, Astral's Python tool manager: on the PATH, or where its installer puts it (~\.local\bin). When it is absent, the
# release archive for this machine from Astral's GitHub releases, refused unless its SHA-256 is the one published beside
# it, unpacked into ~\.local\bin.
function Find-Uv {
  $uv = Get-Command uv -ErrorAction SilentlyContinue
  if ($uv) { return $uv.Source }
  $local = Join-Path $env:USERPROFILE '.local\bin\uv.exe'
  if (Test-Path $local) { Add-ToPath (Split-Path $local); return $local }
  return $null
}

function Install-Uv {
  $arch = if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64') { 'aarch64' } else { 'x86_64' }
  $asset = "uv-$arch-pc-windows-msvc.zip"
  Write-Installing 'uv' "$asset from Astral's GitHub releases"
  $scratch = New-ScratchDir
  $url = "https://github.com/astral-sh/uv/releases/latest/download/$asset"
  $sha = (Get-Text "$url.sha256").Trim().Split(' ')[0]
  Get-VerifiedFile $url $sha (Join-Path $scratch $asset)
  Expand-Archive -Path (Join-Path $scratch $asset) -DestinationPath (Join-Path $scratch 'uv') -ErrorAction Stop
  $bin = Join-Path $env:USERPROFILE '.local\bin'
  New-Item -ItemType Directory -Force -Path $bin -ErrorAction Stop | Out-Null
  Get-ChildItem (Join-Path $scratch 'uv') -Filter '*.exe' -Recurse | Copy-Item -Destination $bin -Force -ErrorAction Stop
  Remove-Item -Recurse -Force $scratch
  Add-ToPath $bin
  return (Join-Path $bin 'uv.exe')
}

# The version an executable's --version reports ("clang-format version 22.1.8 (...)"), or nothing.
function Get-ToolVersion([string]$exe) {
  $line = & $exe --version 2>$null | Select-Object -First 1
  if ($line -match 'version (\d+(\.\d+)+)') { return $Matches[1] }
  return $null
}

# A native command's first line of output, stderr included (Graphviz prints its version there), or '' when the
# command is not there or fails.
function Get-Output([string]$exe, [string[]]$arguments) {
  if (-not $exe -or -not (Get-Command $exe -ErrorAction SilentlyContinue)) { return '' }
  $lines = @(& $exe @arguments 2>&1 | ForEach-Object { "$_".Trim() } | Where-Object { $_ })
  if ($LASTEXITCODE -ne 0 -or -not $lines) { return '' }
  return $lines[0]
}

# Where a command resolves on this process's PATH, or $null.
function Find-Command([string]$name) {
  $found = Get-Command $name -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($found) { return $found.Source }
  return $null
}

function Set-GitHooks {
  if (-not (Test-Path '.githooks')) { return }
  $current = git config core.hooksPath
  if ($current -eq '.githooks') { Write-Ok 'git hooks' 'core.hooksPath .githooks'; return }
  if ($Check) { Write-Missing 'git hooks' 'core.hooksPath is not .githooks'; return }
  git config core.hooksPath .githooks
  Write-Installing 'git hooks' 'core.hooksPath .githooks'
}

# The extensions .vscode/extensions.json recommends, when VS Code's command line is on the PATH.
function Install-VSCodeExtensions {
  $code = Get-Command code -ErrorAction SilentlyContinue
  if (-not $code -or -not (Test-Path '.vscode/extensions.json')) { return }
  $json = (Get-Content '.vscode/extensions.json' -Raw) -replace '(?m)^\s*//.*$', '' | ConvertFrom-Json
  $installed = & $code.Source --list-extensions 2>$null
  foreach ($ext in $json.recommendations) {
    if ($installed -contains $ext) { Write-Ok 'VS Code extension' $ext; continue }
    if ($Check) { Write-Missing 'VS Code extension' $ext; continue }
    Write-Installing 'VS Code extension' $ext
    & $code.Source --install-extension $ext --force | Out-Null
  }
}

# ---------------------------------------------------------------------------------------------------------------------
Write-Host "pawnstar: $(if ($Check) { 'checking' } else { 'setting up' }) the Windows development environment"

# Git for Windows' bash: the UCI suite needs a bash that reads C:\ paths, never WSL's System32\bash.exe (CMakeLists.txt
# looks for Git's first).
$gitBash = $null
$git = Get-Command git -ErrorAction SilentlyContinue
if ($git) {
  $root = (Split-Path (Split-Path (Split-Path (git --exec-path))))
  foreach ($candidate in "$root\bin\bash.exe", "$env:ProgramFiles\Git\bin\bash.exe") {
    if (Test-Path $candidate) { $gitBash = $candidate; break }
  }
}
if ($gitBash) { Write-Ok 'Git for Windows' "$(git --version) ($gitBash)" }
elseif ($Check) { Write-Missing 'Git for Windows' 'git and its bash' }
else {
  Write-Installing 'Git for Windows' 'through winget'
  winget install --id Git.Git -e --accept-package-agreements --accept-source-agreements
  Write-Host '  Git for Windows is installed: open a new terminal and run this script again.'
  exit 0
}

# Visual Studio: tools\build.ps1 takes the latest instance with the x64 C++ tools, for the MSVC STL, the Windows SDK and
# CMake with Ninja, which its developer environment puts on the PATH. The engine is built with clang (cl cannot
# assemble the .incbin embeds): that instance's own when it has the Clang component, else a standalone LLVM that
# targets the MSVC ABI - C:\Program Files\LLVM, or clang++ on the PATH - as CI's Windows runners have. Either does; the
# component is installed only when there is neither.
function Find-StandaloneClang {
  $candidates = @(Join-Path $env:ProgramFiles 'LLVM\bin\clang++.exe') +
    @(Get-Command clang++ -CommandType Application -ErrorAction SilentlyContinue | ForEach-Object Source)
  foreach ($candidate in $candidates) {
    if (-not (Test-Path $candidate)) { continue }
    if ((& $candidate -dumpmachine 2>$null) -match '-windows-msvc$') { return $candidate }
  }
  return $null
}
$vsCpp = 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64'
$vsClangComponent = 'Microsoft.VisualStudio.Component.VC.Llvm.Clang'
$vsComponents = @($vsCpp, 'Microsoft.VisualStudio.Component.VC.CMake.Project')
$vsBuild = Find-VisualStudio @($vsCpp)
$vsComplete = Find-VisualStudio $vsComponents
$sdk = Test-Path "${env:ProgramFiles(x86)}\Windows Kits\10\Include\10.*"
$vsClang = if ($vsBuild) { Join-Path $vsBuild 'VC\Tools\Llvm\x64\bin\clang++.exe' }
$clang = if ($vsClang -and (Test-Path $vsClang)) { $vsClang } else { Find-StandaloneClang }
$clangNote = if (-not $clang) { $null } elseif ($clang -eq $vsClang) { "Visual Studio's clang $(Get-ToolVersion $clang)" }
else { "standalone clang $(Get-ToolVersion $clang) at $clang" }
if ($vsBuild -and $vsBuild -eq $vsComplete -and $sdk -and $clang) {
  Write-Ok 'Visual Studio' "$vsBuild (CMake, Ninja)"
  Write-Ok 'clang' $clangNote
}
elseif ($Check) {
  if (-not $vsBuild) { Write-Missing 'Visual Studio' 'with the x64 C++ tools, a Windows SDK, CMake and Ninja' }
  else {
    $lacking = @($vsComponents | Where-Object { (Find-VisualStudio @($vsCpp, $_)) -ne $vsBuild })
    if (-not $sdk) { $lacking += 'a Windows SDK' }
    if ($lacking) { Write-Missing 'Visual Studio' "$vsBuild lacks $($lacking -join ', ')" }
    else { Write-Ok 'Visual Studio' "$vsBuild (CMake, Ninja)" }
  }
  if ($clang) { Write-Ok 'clang' $clangNote }
  else { Write-Missing 'clang' "Visual Studio's 'C++ Clang tools for Windows' component, or a standalone LLVM" }
}
else {
  $add = $vsComponents + 'Microsoft.VisualStudio.Component.Windows11SDK.26100'
  if (-not $clang) { $add += $vsClangComponent }
  Install-VisualStudio $add
}

# clang-format 22.1.8, the version CI's format job installs (.github/workflows/ci.yml): any first on the PATH that
# reports it, else the PyPI wheel as a uv tool.
$formatVersion = '22.1.8'
$uvToolBin = Join-Path $env:USERPROFILE '.local\bin'
$uv = Find-Uv
if ($uv) {
  $dir = (& $uv tool dir --bin 2>$null | Select-Object -First 1)
  if ($dir) { $uvToolBin = $dir.Trim() }
}
$firstFormat = Get-Command clang-format -ErrorAction SilentlyContinue
$firstVersion = if ($firstFormat) { Get-ToolVersion $firstFormat.Source }
if ($firstVersion -eq $formatVersion) { Write-Ok 'clang-format' "$formatVersion ($($firstFormat.Source))" }
else {
  $uvFormat = Join-Path $uvToolBin 'clang-format.exe'
  if (-not ((Test-Path $uvFormat) -and (Get-ToolVersion $uvFormat) -eq $formatVersion)) {
    if ($Check) { Write-Missing 'clang-format' "$formatVersion (uv tool install clang-format==$formatVersion)"; $uvFormat = $null }
    else {
      if (-not $uv) { $uv = Install-Uv }
      Write-Installing 'clang-format' "$formatVersion, uv tool install clang-format==$formatVersion"
      & $uv tool install "clang-format==$formatVersion"
      if ($LASTEXITCODE -ne 0) { throw "uv tool install exited with $LASTEXITCODE" }
      $uvToolBin = (& $uv tool dir --bin | Select-Object -First 1).Trim()
      $uvFormat = Join-Path $uvToolBin 'clang-format.exe'
    }
  }
  if ($uvFormat) {
    Add-ToPath (Split-Path $uvFormat)
    Write-Ok 'clang-format' "$(Get-ToolVersion $uvFormat) ($uvFormat)"
    # A new shell's PATH is the machine's then the user's: warn when the first clang-format there is another version.
    $newShellPath = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
    foreach ($dir in ($newShellPath -split ';' | Where-Object { $_ })) {
      $other = Join-Path ([Environment]::ExpandEnvironmentVariables($dir)) 'clang-format.exe'
      if (-not (Test-Path $other)) { continue }
      $otherVersion = Get-ToolVersion $other
      if ($otherVersion -ne $formatVersion) {
        Write-Note 'clang-format' "$other ($otherVersion) comes first on a new shell's PATH and shadows it: remove it"
      }
      break
    }
  }
}

# Doxygen and Graphviz's dot, for the doc target (cmake/run_doxygen.cmake; the Doxyfile has HAVE_DOT), each in the
# user's own profile from its project's Windows zip, refused unless its SHA-256 is the pinned one: Doxygen in
# %LOCALAPPDATA%\Programs\doxygen-<version>, Graphviz in %LOCALAPPDATA%\Programs\graphviz. No administrator, no
# installer; the same pins and places as the other reckless-technology repos' setup scripts.
if ($Docs) {
  $doxygenVersion = '1.14.0'
  $doxygenSha256 = '3843742c604e145dab26f74ebd386af0656bc2feb6f834c12c1abb7b3c019d8b'
  $graphvizVersion = '16.1.0'
  $graphvizSha256 = '733e49626c492242eb8dca30ea627b6ead20710e207998c7933b4909d92d6abc'

  $doxygenDir = Join-Path $env:LOCALAPPDATA "Programs\doxygen-$doxygenVersion"
  if (Test-Path (Join-Path $doxygenDir 'doxygen.exe')) { Add-ToPath $doxygenDir }
  $doxygen = Find-Command doxygen
  $have = Get-Output $doxygen @('--version')
  if ($have -like "$doxygenVersion*") { Write-Ok 'Doxygen' "$($have.Split(' ')[0]) ($doxygen)" }
  elseif ($Check) { Write-Missing 'Doxygen' $(if ($doxygen) { "found $doxygen ($have); wants $doxygenVersion" } else { "wants $doxygenVersion" }) }
  else {
    Write-Installing 'Doxygen' "$doxygenVersion into $doxygenDir"
    $scratch = New-ScratchDir
    $zip = Join-Path $scratch 'doxygen.zip'
    Get-VerifiedFile "https://github.com/doxygen/doxygen/releases/download/Release_$($doxygenVersion -replace '\.', '_')/doxygen-$doxygenVersion.windows.x64.bin.zip" $doxygenSha256 $zip
    Expand-Archive $zip -DestinationPath $doxygenDir -Force -ErrorAction Stop
    Remove-Item -Recurse -Force $scratch
    Add-ToPath $doxygenDir
    $doxygen = Find-Command doxygen
    if ($doxygen -ne (Join-Path $doxygenDir 'doxygen.exe')) { Write-Missing 'Doxygen' "installed in $doxygenDir, but $doxygen comes first on the PATH" }
    else { Write-Ok 'Doxygen' "$((Get-Output $doxygen @('--version')).Split(' ')[0]) ($doxygen)" }
  }

  $graphvizBin = Join-Path $env:LOCALAPPDATA 'Programs\graphviz\bin'
  if (Test-Path (Join-Path $graphvizBin 'dot.exe')) { Add-ToPath $graphvizBin }
  $dot = Find-Command dot
  if ($dot) { Write-Ok 'Graphviz' "$(Get-Output $dot @('-V')) ($dot)" }
  elseif ($Check) { Write-Missing 'Graphviz' "dot, for Doxygen's diagrams (wants $graphvizVersion)" }
  else {
    Write-Installing 'Graphviz' "$graphvizVersion into $(Split-Path $graphvizBin)"
    $scratch = New-ScratchDir
    $zip = Join-Path $scratch 'graphviz.zip'
    Get-VerifiedFile "https://gitlab.com/api/v4/projects/4207231/packages/generic/graphviz-releases/$graphvizVersion/windows_10_cmake_Release_Graphviz-$graphvizVersion-win64.zip" $graphvizSha256 $zip
    Expand-Archive $zip -DestinationPath $scratch -ErrorAction Stop
    $target = Split-Path $graphvizBin
    if (Test-Path $target) { Remove-Item -Recurse -Force $target -ErrorAction Stop }
    New-Item -ItemType Directory -Force -Path (Split-Path $target) -ErrorAction Stop | Out-Null
    Move-Item (Join-Path $scratch "Graphviz-$graphvizVersion-win64") $target -ErrorAction Stop
    Remove-Item -Recurse -Force $scratch
    Add-ToPath $graphvizBin
    $dot = Find-Command dot
    Write-Ok 'Graphviz' "$(Get-Output $dot @('-V')) ($dot)"
  }
}

# NNUE training (nnue/README.md): the bullet trainer is Rust, built with cargo, on CUDA when there is an NVIDIA GPU.
if ($Training) {
  $cargoHome = if ($env:CARGO_HOME) { $env:CARGO_HOME } else { Join-Path $env:USERPROFILE '.cargo' }
  $cargoBin = Join-Path $cargoHome 'bin'
  if (Test-Path (Join-Path $cargoBin 'rustup.exe')) { Add-ToPath $cargoBin }
  if (Get-Command rustup -ErrorAction SilentlyContinue) { Write-Ok 'rustup' ((rustup --version 2>$null) | Select-Object -First 1) }
  elseif ($Check) { Write-Missing 'rustup' 'and the stable toolchain, for bullet' }
  else {
    Write-Installing 'rustup' 'rustup-init from static.rust-lang.org'
    $scratch = New-ScratchDir
    $url = 'https://static.rust-lang.org/rustup/dist/x86_64-pc-windows-msvc/rustup-init.exe'
    $sha = (Get-Text "$url.sha256").Split(' ')[0].Trim()
    Get-VerifiedFile $url $sha (Join-Path $scratch 'rustup-init.exe')
    & (Join-Path $scratch 'rustup-init.exe') -y --default-host x86_64-pc-windows-msvc --default-toolchain none --no-modify-path | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "rustup-init exited with $LASTEXITCODE" }
    Remove-Item -Recurse -Force $scratch
    Add-ToPath $cargoBin
  }
  if (Get-Command rustup -ErrorAction SilentlyContinue) {
    # bullet's manifest is edition 2024, which needs a recent cargo: the stable toolchain, not an old pinned one.
    if ((rustup toolchain list 2>$null) -match '^stable') { Write-Ok 'Rust stable' (rustc +stable --version) }
    elseif ($Check) { Write-Missing 'Rust stable' 'rustup toolchain install stable' }
    else {
      Write-Installing 'Rust stable' 'rustup toolchain install stable --profile minimal'
      rustup toolchain install stable --profile minimal
      if ($LASTEXITCODE -ne 0) { throw "rustup exited with $LASTEXITCODE" }
    }
  }

  # CUDA is checked, never installed: the toolkit is large, wants an administrator and must suit the driver.
  $smi = Get-Command nvidia-smi -ErrorAction SilentlyContinue
  $gpu = if ($smi) { & $smi.Source --query-gpu=name,driver_version --format=csv,noheader 2>$null | Select-Object -First 1 }
  if (-not $gpu) { Write-Note 'NVIDIA GPU' 'none found: bullet trains on the CPU with BULLET_FEATURES= (slowly)' }
  else {
    Write-Ok 'NVIDIA GPU' $gpu
    $nvcc = if ($env:CUDA_PATH) { Join-Path $env:CUDA_PATH 'bin\nvcc.exe' }
    if ($nvcc -and (Test-Path $nvcc)) {
      $release = (& $nvcc --version) -match 'release (\d+(\.\d+)+)' | Select-Object -First 1
      if ($release -match 'release (\d+(\.\d+)+)') { $release = $Matches[1] }
      Write-Ok 'CUDA toolkit' "$release ($env:CUDA_PATH)"
    }
    else {
      Write-Missing 'CUDA toolkit' 'CUDA_PATH with nvcc: install it from https://developer.nvidia.com/cuda-downloads (12.3 or later)'
    }
  }

  # bullet at the commit tools/setup_bullet.sh pins, with Pawnstar's examples installed - that script does it, under
  # Git's bash, so there is one pin and one recipe.
  $pin = (Select-String -Path 'tools/setup_bullet.sh' -Pattern '^BULLET_COMMIT=(\w+)').Matches[0].Groups[1].Value
  $bullet = if ($env:BULLET) { $env:BULLET } else { Join-Path $env:USERPROFILE 'pawnstar_nnue\bullet' }
  $bullet = $bullet -replace '\\', '/'
  $head = if (Test-Path "$bullet/.git") { git -C $bullet rev-parse HEAD 2>$null }
  $ready = $head -eq $pin -and (Test-Path "$bullet/examples/pawnstar.rs") -and (Test-Path "$bullet/examples/pawnstar_eval.rs")
  if ($ready) { Write-Ok 'bullet' "$bullet at $($pin.Substring(0, 12))" }
  elseif ($Check) { Write-Missing 'bullet' "$bullet at $($pin.Substring(0, 12)) (tools/setup_bullet.sh)" }
  elseif (-not $gitBash) { Write-Missing 'bullet' 'needs Git for Windows: run this script again in a new terminal' }
  else {
    Write-Installing 'bullet' "$bullet at $($pin.Substring(0, 12)), through tools/setup_bullet.sh"
    $env:BULLET = $bullet
    & $gitBash tools/setup_bullet.sh
    if ($LASTEXITCODE -ne 0) { throw "tools/setup_bullet.sh exited with $LASTEXITCODE" }
  }
}

Set-GitHooks
Install-VSCodeExtensions

if ($Check) {
  if ($script:missing) { Write-Host "$($script:missing) missing: run tools\setup-dev.ps1 (VS Code: the 'setup environment' task)."; exit 1 }
  Write-Host 'Everything is in place.'
}
else {
  Write-Host 'Done. Open a new terminal (or restart VS Code) for PATH changes to reach it, then: tools\build.ps1 check'
}
