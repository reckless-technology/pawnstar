<#
.SYNOPSIS
  The Makefile's targets for a Windows shell: build, test and clean Pawnstar with clang.

.DESCRIPTION
  CMake is Pawnstar's build system of record (CMakeLists.txt). The Makefile wraps it for Unix shells; this script
  wraps it for Windows, target for target and knob for knob. It finds Visual Studio with vswhere, loads the x64
  developer environment (vcvars64.bat) into this process for the MSVC STL, the Windows SDK, CMake and Ninja, picks
  the clang to build with - Visual Studio's own when it has the Clang component, else a standalone LLVM that targets
  the MSVC ABI (C:\Program Files\LLVM, or clang++ on the PATH), as CI's Windows job uses; the engine needs clang,
  and cl cannot assemble the .incbin embeds - puts it first on the PATH and configures the build tree with Ninja, as
  CI's Windows job does. VS Code's tasks run it on Windows.

.PARAMETER Target
  all (the engine alone, the default), tools, tests, check (everything, then every ctest), coverage (everything
  instrumented, then every ctest and the coverage report; implies -Coverage), doc or clean.

.PARAMETER Release
  Omit the DEBUGX diagnostic counters (the Makefile's RELEASE=1, -DPAWNSTAR_DEBUGX=OFF).

.PARAMETER Debug
  A Debug build with AddressSanitizer and UBSan (the Makefile's DEBUG=1). The build copies the sanitizer runtime
  (clang_rt.asan_dynamic-x86_64.dll) beside the executables, so the tests and a hand-run binary need no PATH change.

.PARAMETER Werror
  Warnings as errors (the Makefile's WERROR=1, as CI builds).

.PARAMETER Coverage
  Instrument for source-based code coverage (the Makefile's COVERAGE=1, -DPAWNSTAR_COVERAGE=ON). The coverage target
  runs every ctest and reports src/'s coverage with llvm-cov, with build\coverage\coverage.lcov and an HTML report
  in build\coverage\html, and fails when src/'s line coverage is under the gate (PAWNSTAR_COVERAGE_MIN_LINES, 90).

.PARAMETER CoverageMinLines
  Override the coverage gate, src/'s minimum line coverage in percent (the Makefile's COVERAGE_MIN_LINES,
  -DPAWNSTAR_COVERAGE_MIN_LINES); 0 reports without gating. Left out, the tree keeps its value (90 when new).

.EXAMPLE
  tools\build.ps1             # make
  tools\build.ps1 check       # make check
  tools\build.ps1 -Release    # make RELEASE=1
  tools\build.ps1 -Debug      # make DEBUG=1
  tools\build.ps1 coverage    # make coverage
  tools\build.ps1 clean       # make clean
#>
# No [CmdletBinding()]: it would add PowerShell's common -Debug switch, which this script's own -Debug mirrors make's.
param(
  [ValidateSet('all', 'tools', 'tests', 'check', 'coverage', 'doc', 'clean')]
  [string]$Target = 'all',
  [switch]$Release,
  [switch]$Debug,
  [switch]$Werror,
  [switch]$Coverage,
  [string]$CoverageMinLines = '',
  [string]$BuildDir = 'build',
  [string]$CtestArgs = '-V'
)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)
if ($Target -eq 'coverage') { $Coverage = $true }

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

# The clang to build with: Visual Studio's own (its 'C++ Clang tools for Windows' component) when it has one, else a
# standalone LLVM - its installer's C:\Program Files\LLVM, or the first clang++ on the PATH - as long as it targets the
# MSVC ABI (x86_64-pc-windows-msvc), which links against the STL and SDK the developer environment provides. CI's
# Windows job builds with the hosted image's standalone LLVM.
function Find-Clang {
  $vsClang = Join-Path $env:VSINSTALLDIR 'VC\Tools\Llvm\x64\bin\clang++.exe'
  if (Test-Path $vsClang) { return $vsClang }
  $candidates = @(Join-Path $env:ProgramFiles 'LLVM\bin\clang++.exe') +
    @(Get-Command clang++ -CommandType Application -ErrorAction SilentlyContinue | ForEach-Object Source)
  foreach ($candidate in $candidates) {
    if (-not (Test-Path $candidate)) { continue }
    $target = & $candidate -dumpmachine 2>$null
    if ($target -match '-windows-msvc$') { return $candidate }
  }
  throw "No clang for the MSVC ABI: add Visual Studio's 'C++ Clang tools for Windows' component, or install LLVM."
}
$clang = Find-Clang
Write-Host "pawnstar: building with $clang"
$env:PATH = "$(Split-Path $clang);$env:PATH"

$cmakeArgs = @('-S', '.', '-B', $BuildDir, '-G', 'Ninja', "-DCMAKE_CXX_COMPILER=$($clang -replace '\\', '/')")
if ($Debug) { $cmakeArgs += '-DCMAKE_BUILD_TYPE=Debug', '-DPAWNSTAR_SANITIZE=ON' }
else { $cmakeArgs += '-DCMAKE_BUILD_TYPE=Release', '-DPAWNSTAR_SANITIZE=OFF' }
$cmakeArgs += $(if ($Release) { '-DPAWNSTAR_DEBUGX=OFF' } else { '-DPAWNSTAR_DEBUGX=ON' })
$cmakeArgs += $(if ($Werror) { '-DWERROR=ON' } else { '-DWERROR=OFF' })
$cmakeArgs += $(if ($Coverage) { '-DPAWNSTAR_COVERAGE=ON' } else { '-DPAWNSTAR_COVERAGE=OFF' })
if ($CoverageMinLines -ne '') { $cmakeArgs += "-DPAWNSTAR_COVERAGE_MIN_LINES=$CoverageMinLines" }

function Invoke-Checked([string]$exe, [string[]]$arguments) {
  & $exe @arguments
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Invoke-Checked cmake $cmakeArgs
$tools = 'gen_data', 'gen_openings', 'stamp_net', 'filter_book', 'nnue_quant_study', 'dump_magics'
$tests = 'test_perft', 'test_see', 'test_bratko_kopec_nnue', 'test_nnue', 'test_nnue_incremental',
  'test_opening_book', 'test_chess_clock', 'test_bench_golden', 'test_uci_commands', 'test_primitives',
  'test_principal_variation'
switch ($Target) {
  'all'   { Invoke-Checked cmake @('--build', $BuildDir, '--parallel', '--target', 'pawnstar') }
  'tools' { Invoke-Checked cmake (@('--build', $BuildDir, '--parallel', '--target') + $tools) }
  'tests' { Invoke-Checked cmake (@('--build', $BuildDir, '--parallel', '--target') + $tests) }
  'doc'   { Invoke-Checked cmake @('--build', $BuildDir, '--target', 'doc') }
  'check' {
    Invoke-Checked cmake @('--build', $BuildDir, '--parallel')
    Invoke-Checked ctest (@('--test-dir', $BuildDir) + ($CtestArgs -split ' '))
  }
  'coverage' {
    Invoke-Checked cmake @('--build', $BuildDir, '--parallel')
    Invoke-Checked cmake @('--build', $BuildDir, '--target', 'coverage')
  }
}
