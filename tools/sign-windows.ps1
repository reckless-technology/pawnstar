# Authenticode-sign the release's Windows files with the organisation's code-signing certificate, held in SSL.com's cloud
# (eSigner), through SSL.com's CodeSignTool. The organisation's apps sign the same way, with the same organisation
# secrets (vst-plugins' tools/esigner-sign.ps1, which this mirrors; that repository is private, so the steps are written
# again here).
#
#   pwsh -NoProfile -File tools/sign-windows.ps1 -Check           signs in and lists the account's certificates, which
#                                                                 spends no signing: run before the build, so a wrong
#                                                                 secret shows in a minute
#   pwsh -NoProfile -File tools/sign-windows.ps1 <file>...        each file signed in place with SSL.com's RFC 3161
#                                                                 timestamp, then held as Windows checks it: Valid, in
#                                                                 Jonathan Reckless's name, timestamped
#
# Every signing spends the SSL.com account's allowance: the release signs only in a run that publishes, and only the
# files it ships. The environment, the organisation secrets: ESIGNER_USERNAME, ESIGNER_PASSWORD, ESIGNER_TOTP_SECRET
# (the enrolment's OTP secret, so no one need be at a phone) and, where the account holds several certificates,
# ESIGNER_CREDENTIAL_ID; RECKLESS_CODESIGNTOOL is the folder the release unpacked CodeSignTool into. The tool's own Java
# runs its jar from that folder - not CodeSignTool.bat, since cmd.exe mangles a password holding % ^ & ! or a quote.
param([switch] $Check, [Parameter(ValueFromRemainingArguments = $true)] [string[]] $Files)
$ErrorActionPreference = 'Stop'

$required = @('ESIGNER_USERNAME', 'ESIGNER_PASSWORD', 'RECKLESS_CODESIGNTOOL')
if (-not $Check) { $required += 'ESIGNER_TOTP_SECRET' }
foreach ($name in $required) {
    if (-not [Environment]::GetEnvironmentVariable($name)) { throw "$name is not set: the eSigner secrets (the script's head says which)" }
}
$tool = $env:RECKLESS_CODESIGNTOOL
$java = Get-ChildItem -Path $tool -Filter java.exe -Recurse -File | Where-Object { $_.Directory.Name -eq 'bin' } |
        Select-Object -First 1
$jar = Get-ChildItem -Path (Join-Path $tool 'jar') -Filter 'code_sign_tool-*.jar' -File | Select-Object -First 1
if (-not $java -or -not $jar) { throw "$tool holds no CodeSignTool: no bin\java.exe or jar\code_sign_tool-*.jar" }

# CodeSignTool run with $Arguments from its own folder (it reads conf\ there): what it said, the secrets blanked, and
# its exit code.
function Invoke-CodeSignTool([string[]] $Arguments) {
    Push-Location $tool
    try {
        $said = & $java.FullName -jar $jar.FullName @Arguments 2>&1 | Out-String
        $code = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    foreach ($secret in $env:ESIGNER_PASSWORD, $env:ESIGNER_TOTP_SECRET) {
        if ($secret) { $said = $said -replace [regex]::Escape($secret), '***' }
    }
    Write-Host $said.Trim()
    return [pscustomobject]@{ Said = $said; Code = $code }
}

if ($Check) {
    $result = Invoke-CodeSignTool @('get_credential_ids', "-username=$env:ESIGNER_USERNAME", "-password=$env:ESIGNER_PASSWORD")
    $uuid = '[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}'
    $ids = @([regex]::Matches($result.Said, $uuid) | ForEach-Object Value | Select-Object -Unique)
    if ($result.Code -ne 0 -or $result.Said -match '(?i)\berror\b' -or $ids.Count -eq 0) {
        throw "eSigner refused the sign-in or holds no certificate: ESIGNER_USERNAME and ESIGNER_PASSWORD are the SSL.com account's"
    }
    if ($env:ESIGNER_CREDENTIAL_ID) {
        if ($ids -notcontains $env:ESIGNER_CREDENTIAL_ID) { throw "ESIGNER_CREDENTIAL_ID is none of the account's certificates" }
    } elseif ($ids.Count -gt 1) {
        throw "the account holds $($ids.Count) certificates: set ESIGNER_CREDENTIAL_ID to the code-signing one's id"
    }
    Write-Host "eSigner: signed in; $($ids.Count) certificate(s), the one to sign with known"
    exit 0
}

if (-not $Files) { throw "name the files to sign, or -Check" }
foreach ($file in $Files) {
    $path = (Resolve-Path $file).Path
    $arguments = @('sign', "-username=$env:ESIGNER_USERNAME", "-password=$env:ESIGNER_PASSWORD",
                   "-totp_secret=$env:ESIGNER_TOTP_SECRET", "-input_file_path=$path", '-override')
    if ($env:ESIGNER_CREDENTIAL_ID) { $arguments += "-credential_id=$env:ESIGNER_CREDENTIAL_ID" }
    $result = Invoke-CodeSignTool $arguments
    # The tool can exit 0 having failed, so its words are read too.
    if ($result.Code -ne 0 -or $result.Said -match '(?i)\berror\b') { throw "eSigner did not sign $path" }

    $signature = Get-AuthenticodeSignature -FilePath $path
    if ($signature.Status -ne 'Valid') { throw "$path is not validly signed: $($signature.Status) $($signature.StatusMessage)" }
    if ($signature.SignerCertificate.Subject -notmatch 'CN=Jonathan Reckless') {
        throw "$path is signed by $($signature.SignerCertificate.Subject), not Jonathan Reckless"
    }
    if (-not $signature.TimeStamperCertificate) { throw "$path carries no timestamp" }
    Write-Host "signed $(Split-Path -Leaf $path): $($signature.SignerCertificate.Subject); timestamped by $($signature.TimeStamperCertificate.Subject)"
}
