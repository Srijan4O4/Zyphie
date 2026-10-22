param([ValidateSet('Debug','Release')][string]$Configuration='Release',[string]$BuildDir='build/desktop',[switch]$NoTests)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    $qt="$root/.tools/Qt/6.8.3/llvm-mingw_64"
    $env:PATH="$qt/bin;$root/.tools/llvm-mingw-20261020-ucrt-x86_64/bin;$root/.tools/cmake-4.4.4-windows-x86_64/bin;$root/.tools;"+$env:PATH
    $env:VCPKG_DISABLE_METRICS='1'
    $testing=if($NoTests){'OFF'}else{'ON'}
    cmake -S . -B $BuildDir -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration" '-DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-clang++.exe' "-DCMAKE_TOOLCHAIN_FILE=$root/.tools/vcpkg/scripts/buildsystems/vcpkg.cmake" '-DVCPKG_TARGET_TRIPLET=x64-mingw-static' '-DVCPKG_HOST_TRIPLET=x64-mingw-static' '-DZYPH_BUILD_DESKTOP=ON' "-DCMAKE_PREFIX_PATH=$qt" "-DBUILD_TESTING=$testing"
    if($LASTEXITCODE -ne 0){throw 'Configure failed'}
    cmake --build $BuildDir --parallel 4
    if($LASTEXITCODE -ne 0){throw 'Build failed'}
    if(!$NoTests){
        ctest --test-dir $BuildDir --output-on-failure
        if($LASTEXITCODE -ne 0){throw 'Tests failed'}
        Push-Location "$BuildDir/apps/desktop"
        try {
            ./zyph_desktop_tests.exe -platform windows -o native-tests.txt,txt
            if($LASTEXITCODE -ne 0){throw 'Native GUI tests failed'}
        } finally {Pop-Location}
    }
} finally {Pop-Location}
