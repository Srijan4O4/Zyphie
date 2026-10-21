param([ValidateSet('debug','release')][string]$Preset = 'debug')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    $env:PATH = "$projectRoot\.tools\llvm-mingw-20261020-ucrt-x86_64\bin;$projectRoot\.tools\cmake-4.4.4-windows-x86_64\bin;$projectRoot\.tools;" + $env:PATH
    $env:VCPKG_ROOT = "$projectRoot\.tools\vcpkg"
    $env:VCPKG_DISABLE_METRICS = '1'
    cmake --preset $Preset '-DVCPKG_TARGET_TRIPLET=x64-mingw-static' '-DVCPKG_HOST_TRIPLET=x64-mingw-static' '-DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-clang++.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Configure failed' }
    cmake --build --preset $Preset --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    ctest --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
} finally { Pop-Location }
