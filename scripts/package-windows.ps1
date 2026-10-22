param([string]$BuildDir='build/desktop-production',[string]$Output='build/packages/Zyphie-1.0.0-windows-x64')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    $qt="$root/.tools/Qt/6.8.3/llvm-mingw_64"
    $compiler="$root/.tools/llvm-mingw-20261020-ucrt-x86_64"
    $source="$root/.tools/Qt/6.8.3/Src/qtbase"
    $target=[IO.Path]::GetFullPath((Join-Path $root $Output))
    if(!$target.StartsWith("$root\build\",[StringComparison]::OrdinalIgnoreCase)){throw 'Package output must be within repository build directory'}
    if(Test-Path -LiteralPath $target){throw 'Package output already exists; choose a new output'}
    if(!(Test-Path "$source/LICENSES/LGPL-3.0-only.txt")){throw 'Matching QtBase source is required (aqt install-src windows desktop 6.8.3 --archives qtbase -O .tools/Qt)'}
    $env:PATH="$qt/bin;$compiler/bin;"+$env:PATH
    New-Item -ItemType Directory -Path $target | Out-Null
    Copy-Item "$BuildDir/zyph.exe","$BuildDir/apps/desktop/zyphie.exe" $target
    # Qt 6.8.3 windeployqt misclassifies LLVM-MinGW plugins as debug; deploy the
    # audited Widgets dependency set explicitly, then test with a clean PATH.
    Copy-Item "$qt/bin/Qt6Core.dll","$qt/bin/Qt6Gui.dll","$qt/bin/Qt6Widgets.dll" $target
    New-Item -ItemType Directory "$target/platforms","$target/styles" | Out-Null
    Copy-Item "$qt/plugins/platforms/qwindows.dll" "$target/platforms"
    Copy-Item "$qt/plugins/styles/qmodernwindowsstyle.dll" "$target/styles"
    # LLVM-MinGW runtime dependencies are inspected again by package-smoke.py.
    foreach($dll in @('libc++.dll','libunwind.dll','libwinpthread-1.dll')) {
        Copy-Item "$compiler/bin/$dll" $target
    }
    Copy-Item LICENSE $target
    New-Item -ItemType Directory "$target/docs","$target/licenses","$target/sources" | Out-Null
    Copy-Item docs/USER_GUIDE.md,docs/THIRD_PARTY_NOTICES.md,docs/SECURITY.md,docs/BENCHMARKS.md "$target/docs"
    Copy-Item "$source/LICENSES" "$target/licenses/Qt" -Recurse
    Copy-Item "$qt/sbom/qtbase-6.8.3.spdx.json" "$target/licenses"
    Copy-Item "$BuildDir/vcpkg_installed/x64-mingw-static/share/zstd/copyright" "$target/licenses/Zstandard.txt"
    Copy-Item "$compiler/LICENSE.TXT" "$target/licenses/LLVM.txt"
    Copy-Item "$compiler/x86_64-w64-mingw32/share/mingw32/COPYING*" "$target/licenses"
    $seven="$root/.tools/vcpkg/downloads/tools/7zip-26.03-windows/7z.exe"
    & $seven a -tzip -mx=5 "$target/sources/qtbase-6.8.3.zip" "$source/*" | Out-Null
    if($LASTEXITCODE -ne 0){throw 'Qt source bundling failed'}
    @('[Paths]','Plugins=.') | Set-Content "$target/qt.conf"
    $revision=(git rev-parse HEAD).Trim()
    $manifest=[ordered]@{product='Zyphie';version='1.0.0';revision=$revision;qt='6.8.3';compiler='LLVM-MinGW 20261020 / Clang 23.1.4';zstandard='1.5.7';files=@()}
    $manifest.files=@(Get-ChildItem $target -Recurse -File | ForEach-Object { [ordered]@{path=[IO.Path]::GetRelativePath($target,$_.FullName);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()} })
    $manifest | ConvertTo-Json -Depth 5 | Set-Content "$target/manifest.json"
    & $seven a -tzip -mx=5 "$target.zip" "$target" | Out-Null
    if($LASTEXITCODE -ne 0){throw 'Package compression failed'}
    Get-FileHash "$target.zip" -Algorithm SHA256 | Format-List
    Write-Output "Package: $target.zip"
} finally {Pop-Location}
