$ErrorActionPreference = 'Stop'
$url = 'https://github.com/xanasoft/qt-builds/releases/download/v6.8.4-ssl-lgpl/qt-everywhere-6.8.4-Windows_7-MSVC2022-x86_64-release.7z'
$archive = Join-Path $env:RUNNER_TEMP 'legacy-qt.7z'
$destination = Join-Path $env:RUNNER_TEMP 'legacy-qt'
curl.exe --fail --location --retry 3 --output $archive $url
if ($LASTEXITCODE) { throw 'Qt SDK download failed' }
$expected = '94e56b85a640a785415fbd2cacf70be341d16ce09fd96d98bce7e369c933ebc9'
if ((Get-FileHash $archive).Hash -ne $expected) { throw 'Legacy Qt SDK checksum mismatch' }
7z x "-o$destination" -y $archive
if ($LASTEXITCODE) { throw 'Qt SDK extraction failed' }
$config = Get-ChildItem $destination -Recurse -Filter Qt6Config.cmake | Select-Object -First 1
if (!$config) { throw 'Qt6Config.cmake is missing from the SDK' }
$prefix = $config.Directory.Parent.Parent.Parent.FullName
if (!(Test-Path "$prefix/lib/cmake/Qt6Pdf/Qt6PdfConfig.cmake")) {
    throw 'The legacy SDK does not provide Qt PDF'
}
"QT_ROOT_DIR=$prefix" | Out-File -Append -Encoding utf8 $env:GITHUB_ENV
"$prefix/bin" | Out-File -Append -Encoding utf8 $env:GITHUB_PATH
