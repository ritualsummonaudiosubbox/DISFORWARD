DISFORWARD native implementation - 0.4 DEVELOPMENT SOURCE

THIS IS NOT A PRODUCTION INSTALLABLE RELEASE.
The native driver and both EXEs now compile on a GitHub Windows runner using
Microsoft SDK/WDK 10.0.28000.2526 and Visual Studio 2026. Windows generated the
native catalog with no signability errors or warnings. CI artifacts include
DisforwardAudio.sys, .inf and .cat, but remain UNSIGNED DEVELOPMENT builds.
Microsoft signing and live Windows installation/audio tests are still required.

WHAT CHANGED
- New native ROOT\DISFORWARD device and DisforwardAudio.sys package identity.
- Intended recording endpoint: DISFORWARD; send endpoint: DISFORWARD Send.
- Real per-adapter bounded 48 kHz stereo PCM32 transport replaces the Microsoft
  sample's sine-wave capture. No VB-CABLE package, binary, install or renaming.
- A private endpoint identity property is required by the app. A device merely
  named DISFORWARD, including the old renamed VB-CABLE, is not accepted.
- UI reduced to 360 x 174 logical pixels: My Mic / Discord Friend / Mute.
  PIN keeps it topmost; Setup contains only your microphone, Refresh and native
  driver setup. No output selector or third-party routing controls.
- My Mic no longer depends on Discord being open. Only the selected input is
  captured by this application. Mute releases the active stream after a fade.
- Discord Friend detects a single Discord instance when pressed and captures
  that process tree's rendered audio. It does not capture your own mic or the
  entire desktop as its Discord source.

DISCORD LIMIT
This captures Discord playback, including the friend in a private call, plus
any notification, stream, soundboard or other person rendered by Discord.
It does not isolate a named participant from a group call. Keep Discord speaker
output on headphones and disable unwanted app sounds. No private Discord API,
account token, DLL injection or user-account automation is used.

EXPECTED FLOW AFTER DRIVER BUILD, SIGNING AND VERIFICATION
1. Install the properly signed native DISFORWARD driver package once.
2. Select DISFORWARD as your call application's one microphone input.
3. Leave Discord and call-app speakers on headphones.
4. Open DISFORWARD, choose your physical mic in Setup and press My Mic or
   Discord Friend. Mute sends no voice. Starts muted every time.
Windows 11 x64 is the target (native sample INF minimum build 22000).
Audio in the user-mode app is WASAPI float32; native transport is PCM32 at 48 kHz.
Source switching may have a short interruption while the selected client opens.
Topmost is for normal/borderless windows, not exclusive fullscreen/secure desktop.

WINDOWS BUILD REQUIRED
Use Visual Studio Desktop C++, matching Windows SDK and Windows Driver Kit.
In Developer PowerShell run ./build-windows.ps1. It builds unsigned development
artifacts only; it has been run by the Windows CI workflow.
No script disables signature enforcement, changes Secure Boot or installs test
certificates. The driver must be signed through the normal Windows driver
release process before installation on production Windows systems.
For driver testing, use an appropriately provisioned Windows driver test machine.

WHY THE OLD EXE IS NOT THE FIX
0.3 installed/reused and renamed standard VB-CABLE. That did not satisfy the
requirement for a distinct DISFORWARD driver. This source package replaces that
approach and has no VB-CABLE dependency. Do not treat the old 0.3 as compliant.

VALIDATION
See VALIDATION.txt for actual executed tests and remaining release blockers.
Microsoft sample provenance/license are in driver/UPSTREAM.txt and
 driver/LICENSE-MS-PL.txt. Derived driver source is provided under MS-PL.
