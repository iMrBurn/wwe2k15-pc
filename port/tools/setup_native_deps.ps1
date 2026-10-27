# Fetches and patches the two source dependencies of wwe2k15_native.exe (the native renderer):
# plume (Vulkan layer) and reblue-XenosRecomp (shader translator), pinned to the commits WWE '12 PC
# uses so their patches apply. Default folder: C:\Dev\native-deps (cmake: WWE2K15_NATIVE_DEPS).
#   .\port\tools\setup_native_deps.ps1 [-Dir D:\native-deps] [-PlumeUrl <git url>] [-XenosRecompUrl <git url>]
# Safe to run again: a patch already applied is skipped.
param(
  [string]$Dir = "C:\Dev\native-deps",
  [string]$PlumeUrl = "https://github.com/zolaware/plume.git",
  [string]$XenosRecompUrl = "https://github.com/zolaware/reblue-XenosRecomp.git"
)
$ErrorActionPreference = "Stop"
$patches = Join-Path $PSScriptRoot "..\patches" | Resolve-Path

function Invoke-Git { & git @args; if ($LASTEXITCODE) { throw "git $args failed" } }

function Get-PinnedRepo($Url, $Path, $Commit, [string[]]$Patches) {
  if (-not (Test-Path $Path)) { Invoke-Git clone $Url $Path }
  Invoke-Git -C $Path fetch --all --quiet
  Invoke-Git -C $Path checkout --quiet $Commit
  Invoke-Git -C $Path submodule update --init --recursive
  foreach ($name in $Patches) {
    $patch = Join-Path $patches $name
    cmd /c "git -C `"$Path`" apply --reverse --check `"$patch`" 2>nul"
    if ($LASTEXITCODE -eq 0) { continue }  # already applied
    Invoke-Git -C $Path apply --whitespace=nowarn $patch
  }
}

New-Item -ItemType Directory -Force $Dir | Out-Null
# plume-reblue.patch and xenosrecomp-reblue-wwe12.patch come from WWE '12 PC (iMrBurn/wwe12-pc).
Get-PinnedRepo $PlumeUrl (Join-Path $Dir "plume") "e0c8871" `
  @("plume-reblue.patch", "plume-pipeline-cache.patch")
Get-PinnedRepo $XenosRecompUrl (Join-Path $Dir "reblue-XenosRecomp") "339af41" `
  @("xenosrecomp-reblue-wwe12.patch")
Write-Output "Native renderer dependencies ready in $Dir"
