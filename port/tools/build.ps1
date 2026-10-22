# Build WWE 2K15 PC (and optionally the patched ReXGlue SDK) from a plain PowerShell.
#
#   .\port\tools\build.ps1                 # configure + build port/out/build/win-amd64-release
#   .\port\tools\build.ps1 -Sdk            # rebuild/install the SDK first, then the game
#   .\port\tools\build.ps1 -SdkDir D:\rexglue-sdk
#
# The SDK checkout must be ReXGlue c94f5eb with every port/patches/rexglue-sdk-*.patch applied
# except rexglue-sdk-xctd-compression-info.patch (see docs/PORTING_PLAN.md).
param(
  [switch]$Sdk,
  [string]$SdkDir = "C:\Dev\rexglue-sdk"
)
$ErrorActionPreference = "Stop"
$port = Split-Path -Parent $PSScriptRoot
$install = Join-Path $SdkDir "out\install\ninja-clangcl"

# MSVC environment (Windows SDK, linker) + LLVM clang-cl.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
  if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($Matches[1])" $Matches[2] }
}
$env:PATH = "$env:ProgramFiles\LLVM\bin;" + (Split-Path $vswhere) + ";$env:PATH"

if ($Sdk) {
  Push-Location $SdkDir
  try {
    if (-not (Test-Path "out\build\win-amd64\CMakeCache.txt")) {
      cmake --preset win-amd64 -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl `
        -DREXGLUE_USE_VULKAN=ON -DREXGLUE_ENABLE_TRACY=OFF "-DCMAKE_INSTALL_PREFIX=$install" `
        "-DCMAKE_C_FLAGS=/clang:-march=x86-64-v3 /clang:-Wno-everything" `
        "-DCMAKE_CXX_FLAGS=/EHsc /Zc:char8_t- /clang:-march=x86-64-v3 /clang:-Wno-everything"
      if ($LASTEXITCODE) { throw "SDK configure failed" }
    }
    cmake --build out/build/win-amd64 --config Release; if ($LASTEXITCODE) { throw "SDK build failed" }
    cmake --install out/build/win-amd64 --config Release; if ($LASTEXITCODE) { throw "SDK install failed" }
  } finally { Pop-Location }
}

Push-Location $port
try {
  $i = $install -replace '\\', '/'
  cmake --preset win-amd64-release "-DCMAKE_PREFIX_PATH=$i" "-DSDL3_DIR=$i/cmake" `
    "-DWWE2K15_IMGUI_DIR:PATH=$(($SdkDir -replace '\\', '/'))/thirdparty/imgui"
  if ($LASTEXITCODE) { throw "configure failed" }
  cmake --build --preset win-amd64-release; if ($LASTEXITCODE) { throw "build failed" }
  # The game build does not re-copy SDK DLLs when only the SDK changed.
  Copy-Item (Join-Path $install "bin\rexruntime.dll"), (Join-Path $install "bin\rexgpu-xenos.dll") `
    "out\build\win-amd64-release\" -Force
  Copy-Item "wwe2k15_runtime.toml" "out\build\win-amd64-release\wwe2k15.toml" -Force
} finally { Pop-Location }
