# Run from a VS Developer PowerShell with the matching Windows SDK and WDK installed.
# Builds unsigned development artifacts only. This script never changes Windows
# driver-signing policy and never installs a kernel driver.
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
foreach ($tool in @('msbuild.exe','cl.exe','rc.exe')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "Missing $tool. Use Visual Studio Developer PowerShell with Desktop C++, Windows SDK and WDK."
    }
}
$out = Join-Path $root 'artifacts\UNSIGNED-DEVELOPMENT'
New-Item -ItemType Directory -Force $out | Out-Null
New-Item -ItemType Directory -Force (Join-Path $out 'obj') | Out-Null
$driverSolution = Join-Path $root 'driver\SimpleAudioSample.sln'
& msbuild.exe $driverSolution /m /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:SignMode=Off /v:minimal
if ($LASTEXITCODE -ne 0) { throw 'Native driver build failed.' }
$driverOut = Join-Path $out 'driver'
New-Item -ItemType Directory -Force $driverOut | Out-Null
foreach ($name in @('DisforwardAudio.sys','DisforwardAudio.inf','DisforwardAudio.cat')) {
    $file = Get-ChildItem (Join-Path $root 'driver') -Recurse -File -Filter $name |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $file) { throw "Expected driver output missing: $name" }
    Copy-Item $file.FullName (Join-Path $driverOut $name) -Force
}
Push-Location (Join-Path $root 'app')
try {
    & rc.exe /nologo /fo (Join-Path $out 'app.res') app.rc
    if ($LASTEXITCODE -ne 0) { throw 'Console resources failed.' }
    & cl.exe /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /W4 /WX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS console.cpp engine.cpp (Join-Path $out 'app.res') "/Fo$(Join-Path $out 'obj')\" "/Fe:$out\DISFORWARD.exe" /link /MANIFEST:NO /SUBSYSTEM:WINDOWS ole32.lib uuid.lib mmdevapi.lib avrt.lib gdiplus.lib shell32.lib gdi32.lib user32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Console build failed.' }
    & rc.exe /nologo /fo (Join-Path $out 'installer.res') installer.rc
    if ($LASTEXITCODE -ne 0) { throw 'Installer resources failed.' }
    & cl.exe /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /W4 /WX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS driver_setup.cpp (Join-Path $out 'installer.res') "/Fo$(Join-Path $out 'obj')\" "/Fe:$out\DISFORWARD-Driver-Setup.exe" /link /MANIFEST:NO /SUBSYSTEM:WINDOWS setupapi.lib newdev.lib uuid.lib user32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Driver installer build failed.' }
} finally { Pop-Location }
@'
UNSIGNED DEVELOPMENT BUILD - NOT A PRODUCTION RELEASE
The driver must be reviewed, tested on a provisioned Windows driver test machine,
and Microsoft-signed for normal supported Windows installation.
No signing policy is changed by this package. The installer must reject an
unsigned or incomplete driver package on a normal Windows configuration.
'@ | Set-Content (Join-Path $out 'NOT-A-RELEASE.txt')
Remove-Item (Join-Path $out 'obj') -Recurse -Force
Remove-Item (Join-Path $out '*.res') -Force
Get-ChildItem $out -Recurse -File | Where-Object { $_.Extension -in '.exe','.sys','.inf','.cat' } |
    Sort-Object FullName | ForEach-Object {
        $hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $([IO.Path]::GetRelativePath($out,$_.FullName))"
    } | Set-Content (Join-Path $out 'SHA256.txt')
Write-Host "Unsigned development output: $out"
