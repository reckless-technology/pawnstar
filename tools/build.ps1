<#
.SYNOPSIS
  The Makefile's targets for a Windows shell: build, test and clean Pawnstar with Visual Studio's clang.

.DESCRIPTION
  CMake is Pawnstar's build system of record (CMakeLists.txt). The Makefile wraps it for Unix shells; this script
  wraps it for Windows, target for target and knob for knob. It finds Visual Studio with vswhere, loads the x64
  developer environment (vcvars64.bat) into this process for the MSVC STL, the Windows SDK, CMake and Ninja, puts
  Visual Studio's own clang first on the PATH - the engine needs clang, and cl cannot assemble the .incbin embeds -
  and configures the build tree with Ninja, as CI's Windows job does. VS Code's tasks run it on Windows.

.PARAMETER Target
  all (the engine alone, the default), tools, tests, check (everything, then every ctest), doc or clean.

.PARAMETER Release
  Omit the DEBUGX diagnostic counters (the Makefile's RELEASE=1, -DPAWNSTAR_DEBUGX=OFF).

.PARAMETER Debug
  A Debug build with AddressSanitizer and UBSan (the Makefile's DEBUG=1). The sanitizer runtime DLLs are in
  Visual Studio's clang directory, which this script puts on the PATH for the tests it runs.

.PARAMETER Werror
  Warnings as errors (the Makefile's WERROR=1, as CI builds).

.EXAMPLE
  tools\build.ps1             # make
  tools\build.ps1 check       # make check
  tools\build.ps1 -Release    # make RELEASE=1
  tools\build.ps1 -Debug      # make DEBUG=1
  tools\build.ps1 clean       # make clean
#>
# No [CmdletBinding()]: it would add PowerShell's common -Debug switch, which this script's own -Debug mirrors make's.
param(
  [ValidateSet('all', 'tools', 'tests', 'check', 'doc', 'clean')]
  [string]$Target = 'all',
  [switch]$Release,
  [switch]$Debug,
  [switch]$Werror,
  [string]$BuildDir = 'build',
  [string]$CtestArgs = '-V'
)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)

if ($Target -eq 'clean') {
  if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
  exit 0
}

# The developer environment, once per process: a second call finds VSINSTALLDIR set and skips it.
if (-not $env:VSINSTALLDIR) {
  $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
  $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if (-not $vs) { throw 'Visual Studio with the x64 C++ tools was not found (vswhere).' }
  cmd /s /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
  }
}
$llvm = Join-Path $env:VSINSTALLDIR 'VC\Tools\Llvm\x64\bin'
if (-not (Test-Path (Join-Path $llvm 'clang++.exe'))) {
  throw "Visual Studio's clang is not installed: add the 'C++ Clang tools for Windows' component."
}
$env:PATH = "$llvm;$env:PATH"
# The sanitizer runtimes (clang_rt.asan_dynamic-x86_64.dll) for a -Debug build's tests.
$clangLib = Get-ChildItem (Join-Path $env:VSINSTALLDIR 'VC\Tools\Llvm\x64\lib\clang\*\lib\windows') -Directory -ErrorAction SilentlyContinue |
  Select-Object -First 1
if ($clangLib) { $env:PATH = "$($clangLib.FullName);$env:PATH" }

$cmakeArgs = @('-S', '.', '-B', $BuildDir, '-G', 'Ninja', '-DCMAKE_CXX_COMPILER=clang++')
if ($Debug) { $cmakeArgs += '-DCMAKE_BUILD_TYPE=Debug', '-DPAWNSTAR_SANITIZE=ON' }
else { $cmakeArgs += '-DCMAKE_BUILD_TYPE=Release', '-DPAWNSTAR_SANITIZE=OFF' }
$cmakeArgs += $(if ($Release) { '-DPAWNSTAR_DEBUGX=OFF' } else { '-DPAWNSTAR_DEBUGX=ON' })
$cmakeArgs += $(if ($Werror) { '-DWERROR=ON' } else { '-DWERROR=OFF' })

function Invoke-Checked([string]$exe, [string[]]$arguments) {
  & $exe @arguments
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Invoke-Checked cmake $cmakeArgs
$tools = 'stamp_net', 'filter_book', 'nnue_quant_study', 'dump_magics'
$tests = 'test_perft', 'test_see', 'test_bratko_kopec_nnue', 'test_nnue', 'test_nnue_incremental',
  'test_opening_book', 'test_chess_clock'
switch ($Target) {
  'all'   { Invoke-Checked cmake @('--build', $BuildDir, '--parallel', '--target', 'pawnstar') }
  'tools' { Invoke-Checked cmake (@('--build', $BuildDir, '--parallel', '--target') + $tools) }
  'tests' { Invoke-Checked cmake (@('--build', $BuildDir, '--parallel', '--target') + $tests) }
  'doc'   { Invoke-Checked cmake @('--build', $BuildDir, '--target', 'doc') }
  'check' {
    Invoke-Checked cmake @('--build', $BuildDir, '--parallel')
    Invoke-Checked ctest (@('--test-dir', $BuildDir) + ($CtestArgs -split ' '))
  }
}
