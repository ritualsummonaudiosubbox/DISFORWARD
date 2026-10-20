# DISFORWARD

A compact Windows 11 x64 console that switches your physical microphone or
Discord's playback into one call-app microphone input.

This packaged version uses **VB-CABLE by VB-Audio**, with the user's approval.
It includes the original vendor installer and keeps VB-CABLE's device names.
The microphone selected in your call app is **CABLE Output**. A custom unsigned
DISFORWARD driver is not installed or required.

## Use

1. Extract the package and run `DISFORWARD-Setup.exe` once. Install VB-CABLE
   through its original installer and restart Windows. Skip if already installed.
2. Open `DISFORWARD.exe`, open Setup and select your physical mic.
3. Run **Check audio line** outside a live call.
4. Select **CABLE Output (VB-Audio Virtual Cable)** in the call app as its mic.
5. Keep Discord, Windows and call-app playback on headphones. Keep Discord
   input on your physical mic.
6. Press **My Mic**, **Discord Friend**, or **Mute**. **PIN** keeps the compact
   console topmost. Finish with a short test call before customer use.

Discord Friend captures the desktop Discord process tree, including call audio,
notifications and streams. It does not isolate one named participant in a group
call. Use a private call and turn off unwanted Discord sounds.

`START-HERE.txt` contains the complete setup/troubleshooting guide. VB-CABLE is
[donationware](https://vb-audio.com/Services/licensing.htm); donations are welcome,
and its professional licensing terms apply to call-center/business use.

## Build and verification

[Windows builds and packaged downloads](https://github.com/ritualsummonaudiosubbox/DISFORWARD/actions/workflows/windows.yml)

Use x64 Visual Studio Developer PowerShell with Desktop C++ and Windows SDK:

```powershell
./build-windows.ps1
```

The build fetches the original VB-CABLE archive from the vendor, verifies its
pinned SHA256, builds the two EXEs and packages the guides under
`artifacts/DISFORWARD-Windows-x64`. No WDK is required for this version.

CI checks source, audio logic, cable selection, Windows UI controls, the vendor
kernel signature, and attempts a real driver/audio test on its disposable runner.
`WINDOWS-AUDIO-CHECK.txt` records the audio attempt; `VALIDATION.txt` distinguishes
these checks from the required live Discord/physical-microphone/call-app test.

Audio uses 48 kHz stereo float32 WASAPI with quality sample-rate conversion and
short switching fades. PIN works above normal/borderless windows; exclusive
fullscreen and secure desktops are not guaranteed.

The earlier native driver source remains for future work. It is not part of this
package; that separate implementation still needs Microsoft signing and kernel
runtime testing. Its provenance/license are retained in `driver/`.
