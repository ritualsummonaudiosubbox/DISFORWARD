# Build the app against the existing signed VB-CABLE transport.
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
foreach ($tool in @('cl.exe','rc.exe')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "Missing $tool. Use x64 Visual Studio Developer PowerShell." }
}
$vendor = Join-Path $root 'third_party\VBCABLE_Driver_Pack45.zip'
New-Item -ItemType Directory -Force (Split-Path $vendor) | Out-Null
if (-not (Test-Path $vendor)) { Invoke-WebRequest 'https://download.vb-audio.com/Download_CABLE/VBCABLE_Driver_Pack45.zip' -OutFile $vendor }
$expected = 'b950e39f01af1d04ea623c8f6d8eb9b6ea5c477c637295fabf20631c85116bfb'
if ((Get-FileHash $vendor -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) { throw 'Official vendor archive checksum changed. Review before building.' }
$out = Join-Path $root 'artifacts\DISFORWARD-Windows-x64'
New-Item -ItemType Directory -Force (Join-Path $out 'obj') | Out-Null
Push-Location (Join-Path $root 'app')
try {
    & rc.exe /nologo /fo (Join-Path $out 'app.res') app.rc
    if ($LASTEXITCODE) { throw 'Console resources failed.' }
    & cl.exe /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /W4 /WX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS console.cpp engine.cpp diagnostics.cpp (Join-Path $out 'app.res') "/Fo$(Join-Path $out 'obj')\" "/Fe:$out\DISFORWARD.exe" /link /MANIFEST:NO /SUBSYSTEM:WINDOWS ole32.lib uuid.lib mmdevapi.lib avrt.lib gdiplus.lib shell32.lib gdi32.lib user32.lib cfgmgr32.lib
    if ($LASTEXITCODE) { throw 'Console build failed.' }
    & rc.exe /nologo /fo (Join-Path $out 'setup.res') cable_setup.rc
    if ($LASTEXITCODE) { throw 'Setup resources failed.' }
    & cl.exe /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /W4 /WX /DUNICODE /D_UNICODE cable_setup.cpp (Join-Path $out 'setup.res') "/Fo$(Join-Path $out 'obj')\" "/Fe:$out\DISFORWARD-Setup.exe" /link /MANIFEST:NO /SUBSYSTEM:WINDOWS bcrypt.lib shell32.lib user32.lib urlmon.lib ole32.lib uuid.lib cfgmgr32.lib
    if ($LASTEXITCODE) { throw 'VB-CABLE setup launcher build failed.' }
} finally { Pop-Location }
Copy-Item (Join-Path $root 'START-HERE.txt') $out -Force
Copy-Item (Join-Path $root 'THIRD-PARTY.txt') $out -Force
Copy-Item (Join-Path $root 'VALIDATION.txt') $out -Force
Remove-Item (Join-Path $out 'obj') -Recurse -Force
Remove-Item (Join-Path $out '*.res') -Force
Get-ChildItem $out -File -Filter '*.exe' | Sort-Object Name | ForEach-Object {
    "$((Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant())  $($_.Name)"
} | Set-Content (Join-Path $out 'SHA256.txt')
Write-Host "Windows package: $out"
