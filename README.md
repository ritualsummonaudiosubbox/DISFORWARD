# DISFORWARD

A compact Windows microphone switcher for forwarding your microphone or Discord playback into one call-app microphone input.

**Development build — not a production-ready installer.** The native driver compiles with Microsoft's WDK, and Windows generates its catalog. The package still requires Microsoft driver signing and live Windows audio validation. An unsigned build cannot install normally on production Windows.

## Audio routing

- **My Mic** forwards the physical microphone selected in Setup.
- **Discord Friend** captures the desktop Discord process tree's playback. Discord can continue playing through your headphones.
- **Mute** closes the source stream after a brief fade. The app starts muted.
- **PIN** keeps the 360 × 174 console topmost and collapses Setup. Normal and borderless windows are supported; exclusive fullscreen and secure desktop overlays are not guaranteed.

The native driver has its own `ROOT\DISFORWARD` hardware identity, `DisforwardAudio` service, **DISFORWARD Send** playback endpoint, and **DISFORWARD** recording endpoint. It does not install, rename or depend on another virtual cable driver.

After a signed driver has been installed and tested, choose **DISFORWARD** as the call application's microphone. Leave Discord and call-app playback on headphones. **DISFORWARD Send is reserved for the switcher**: selecting it directly as Discord's speaker bypasses the source buttons.

Discord capture includes everything Discord plays: call participants, notifications, streams and soundboards. It cannot isolate one named participant in a group call. Use a private call and disable unwanted Discord sounds. No Discord account token or private API is used.

## Build and download

[Windows build runs and development artifacts](https://github.com/ritualsummonaudiosubbox/DISFORWARD/actions/workflows/windows.yml)

The workflow uses Visual Studio 2026 and pinned Microsoft SDK/WDK NuGet packages. It runs the source checks and PCM/source-switching tests, builds the native driver and both executables, and checks the UI in an audio-disabled preview. The uploaded artifact is explicitly named **DISFORWARD-UNSIGNED-DEVELOPMENT-x64**.

For a local build, open x64 Visual Studio Developer PowerShell with Desktop C++ and the WDK Visual Studio component installed:

```powershell
nuget restore packages.config -PackagesDirectory packages -NonInteractive
./build-windows.ps1
```

Output: `artifacts/UNSIGNED-DEVELOPMENT`. This contains the app, native driver setup, and driver INF/SYS/CAT. It is not a signed release. The installer requests administrator access and verifies the INF catalog before registering a device; Windows also enforces its normal installation policy.

## Audio format and validation

Target: Windows 11 x64. User-mode routing uses WASAPI at 48 kHz stereo float32; the native transport uses 48 kHz stereo PCM32. Source changes use a short fade and open only the selected capture source. There is no lossy codec in DISFORWARD, nor a claim that it can restore quality already lost in Discord or a microphone.

`VALIDATION.txt` records executed checks and remaining Windows installation/audio tests. Unit tests and compilation do not establish kernel-driver safety or prove an end-to-end call works. No production release is published until those checks are completed.

Driver sample provenance and MS-PL license: `driver/UPSTREAM.txt` and `driver/LICENSE-MS-PL.txt`.
