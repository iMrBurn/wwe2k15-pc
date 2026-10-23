# Package a release zip of WWE 2K15 PC (no game data).
#
#   .\port\tools\package.ps1            # uses the existing release build
#   .\port\tools\package.ps1 -Build     # rebuilds first
#
# Output: dist\WWE2K15-PC-v<VERSION>.zip, dist\SHA256-v<VERSION>.txt and
# dist\symbols-v<VERSION>\ (exe + pdb/map kept locally to read crash reports).
param([switch]$Build, [string]$SdkDir = "C:\Dev\rexglue-sdk")
$ErrorActionPreference = "Stop"
$port = Split-Path -Parent $PSScriptRoot
$repo = Split-Path -Parent $port
$version = (Get-Content (Join-Path $repo "VERSION") -TotalCount 1).Trim()
$buildDir = Join-Path $port "out\build\win-amd64-release"
if ($Build) { & (Join-Path $PSScriptRoot "build.ps1") -SdkDir $SdkDir }

$dist = Join-Path $repo "dist"
$name = "WWE2K15-PC-v$version"
$stage = Join-Path $dist $name
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage, (Join-Path $stage "game"), (Join-Path $stage "licenses") | Out-Null

# Program files.
foreach ($f in "wwe2k15.exe", "WWE 2K15 Launcher.exe", "rexruntime.dll", "rexgpu-xenos.dll", "gamecontrollerdb.txt") {
  Copy-Item (Join-Path $buildDir $f) $stage
}
Copy-Item (Join-Path $port "wwe2k15_runtime.toml") (Join-Path $stage "wwe2k15.toml")
Copy-Item (Join-Path $port "dist\game\PUT_GAME_FILES_HERE.txt") (Join-Path $stage "game")
foreach ($f in "LISEZMOI.txt", "README.txt") {
  (Get-Content (Join-Path $port "dist\$f") -Raw -Encoding UTF8).Replace("{VERSION}", $version) |
    Set-Content (Join-Path $stage $f) -Encoding UTF8 -NoNewline
}

# Third-party notices for what is linked into the shipped binaries.
Copy-Item (Join-Path $SdkDir "LICENSE") (Join-Path $stage "licenses\ReXGlue-SDK.txt")
$third = @{ "SDL3" = "sdl3\LICENSE.txt"; "Dear-ImGui" = "imgui\LICENSE.txt"; "fmt" = "fmt\LICENSE";
  "spdlog" = "spdlog\LICENSE"; "toml++" = "tomlplusplus\LICENSE"; "xxHash" = "xxHash\LICENSE";
  "glslang" = "glslang\LICENSE.txt"; "SPIRV-Tools" = "spirv-tools\LICENSE"; "SIMDe" = "simde\COPYING";
  "Vulkan-Memory-Allocator" = "vulkan-memory-allocator\LICENSE.txt"; "Vulkan-Loader" = "vulkan-loader\LICENSE.txt";
  "FFmpeg" = "FFmpeg\LICENSE.md"; "FFmpeg-LGPL-2.1" = "FFmpeg\COPYING.LGPLv2.1"; "utfcpp" = "utfcpp\LICENSE";
  "o1heap" = "o1heap\LICENSE"; "aes_128" = "aes_128\LICENSE" }
foreach ($k in $third.Keys) {
  $src = Join-Path $SdkDir ("thirdparty\" + $third[$k])
  if (Test-Path $src) { Copy-Item $src (Join-Path $stage "licenses\$k.txt") }
}

# Guard: never ship game data.
$bad = Get-ChildItem $stage -Recurse -File | Where-Object { $_.Extension -in ".xex", ".xexp", ".pac", ".pck", ".bk2", ".bik", ".iso" }
if ($bad) { throw "Game data found in the package: $($bad.FullName -join ', ')" }

$zip = Join-Path $dist "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $stage -DestinationPath $zip
$hash = (Get-FileHash $zip -Algorithm SHA256).Hash
"$hash  $name.zip" | Set-Content (Join-Path $dist "SHA256-v$version.txt") -Encoding ASCII

# Keep matching symbols locally (not uploaded) to read crash/hang addresses later.
$sym = Join-Path $dist "symbols-v$version"
New-Item -ItemType Directory -Force $sym | Out-Null
Get-ChildItem $buildDir -File | Where-Object { $_.Extension -in ".pdb", ".map" -or $_.Name -eq "wwe2k15.exe" } |
  Copy-Item -Destination $sym

"Package: $zip"
"SHA-256: $hash"
"Size: {0:N1} MB" -f ((Get-Item $zip).Length / 1MB)
