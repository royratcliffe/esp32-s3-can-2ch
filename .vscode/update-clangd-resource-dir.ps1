$ErrorActionPreference = 'Stop'

$settingsPath = Join-Path $PSScriptRoot 'settings.json'
$settings = Get-Content $settingsPath -Raw | ConvertFrom-Json
$clangdPath = $settings.'clangd.path'

if (-not (Test-Path $clangdPath -PathType Leaf)) {
    throw "clangd.path does not point to an executable: $clangdPath"
}

$versionOutput = (& $clangdPath --version 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0 -or $versionOutput -notmatch 'clangd version (?<major>\d+)') {
    throw "Could not determine clangd's major version from: $versionOutput"
}

$clangdPackage = Split-Path -Parent (Split-Path -Parent $clangdPath)
$versionDirectory = Split-Path -Parent $clangdPackage
$toolsDirectory = Split-Path -Parent (Split-Path -Parent $versionDirectory)
$resourceDirectory = Join-Path $toolsDirectory "esp-clang\$(Split-Path -Leaf $versionDirectory)\esp-clang\lib\clang\$($Matches.major)"

if (-not (Test-Path $resourceDirectory -PathType Container)) {
    throw "clangd resource directory was not found: $resourceDirectory"
}

$arguments = @($settings.'clangd.arguments' | Where-Object { $_ -notlike '--resource-dir=*' })
$settings.'clangd.arguments' = @($arguments) + "--resource-dir=$resourceDirectory"

$json = ConvertTo-Json $settings -Depth 100
Set-Content -Path $settingsPath -Value $json -Encoding UTF8
Write-Output "Set clangd resource directory to: $resourceDirectory"
