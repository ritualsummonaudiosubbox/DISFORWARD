DISFORWARD 0.6.2 - independent Discord receive + SPEAKER
Windows x64. Extract the complete folder before running.

FIRST SETUP
1. Close audio applications. Run DISFORWARD-Setup.exe as administrator.
   Skip the SEND installation if CABLE Output is already present.
   Install the independent VB-Audio Hi-Fi Cable receive line when offered.
   The receive installer downloads from VB-Audio and needs internet access.
   Use the original vendor Install button. Restart Windows after installation.
2. Open DISFORWARD, Setup, Refresh. Choose your physical microphone.
   Choose your headphones/physical speakers under SPEAKER.
3. In Discord Voice & Video:
   INPUT: your physical microphone.
   OUTPUT: Speakers (VB-Audio Hi-Fi Cable).
   Some Windows versions display Hi-Fi Cable Input (VB-Audio Hi-Fi Cable).
   Use the exact receive-device name shown in DISFORWARD Setup.
4. In the phone/call-center app:
   MICROPHONE: CABLE Output (VB-Audio Virtual Cable).
   SPEAKERS: your normal headphones/physical speakers.
   Keep the phone app microphone explicitly selected, not system default.
5. DISFORWARD negotiates matching 48 kHz streams on the receive cable.
   Leave ASIO Bridge closed. Turn off Windows "Listen to this device".
   If the app reports a format lock, close Discord playback and Refresh.
   Fallback: set both Hi-Fi Cable endpoint formats to 48,000 Hz in Windows
   Sound settings > device properties > Advanced, then Refresh.
6. Before joining a live call, use Check audio lines, then a short test call.

DAILY CONTROLS
My Mic          Phone call hears your mic only.
Discord Friend  Phone call hears Discord only.
Mute            Phone call hears neither source.
SPEAKER         Hear Discord on the selected listening device independently
                of all three phone controls. Lit means enabled.
PIN             Compact always-on-top console. Works over normal/borderless
                windows; Windows exclusive-fullscreen apps can cover it.

The app starts muted, with SPEAKER off. No audio is recorded or uploaded.
Changing listening device restarts only listening. Changing mic stops sending.
Refresh stops both routes. Re-select controls after Refresh.

NO WINDOWS DEFAULT-DEVICE CHANGES ARE REQUIRED
Do not use the same cable for receive and send. The receive device above is
independent of CABLE Output. The app does not set Windows playback or
communications defaults. Driver installation can affect Windows device
choices; keep application devices explicitly selected afterward.

ECHO PREVENTION
Keep Discord INPUT on the physical mic, never CABLE Output.
For PC speakers, turn ON Echo Cancellation in Discord Voice & Video.
DISFORWARD cannot process the direct physical mic captured by Discord.
If echo continues, use headphones or a separate echo-cancelled mic endpoint. The SPEAKER path
carries Discord audio only; it never monitors your mic or the phone-call bus.
Headphones prevent sound from physically re-entering the microphone. PC
speakers near a live mic can still cause acoustic echo; software routing
alone cannot guarantee cancellation of that sound.

AUDIO QUALITY
SPEAKER uses a media playback stream with supported RAW processing and opts
out of Windows communications auto-ducking. Outputs below 44.1 kHz are
rejected rather than silently using a narrow-band listening device. RAW
bypasses optional effects only where supported, not always-on hardware DSP.
48 kHz stereo float processing, shared WASAPI high-quality conversion,
click-reducing source transitions and clock-drift compensation. No automatic
noise gate, compression, gain boost or voice filtering alters the signal.
Source/Discord/network/phone codec quality still determines final call quality.

TROUBLESHOOTING
Missing receive line: run the receive installer, restart, then Refresh.
A receive format error: close Discord playback and ASIO Bridge, then Refresh.
The app sets matching stream rates without rewriting default device formats.
No speakers listed: connect headphones/speakers and Refresh.
Do not disable driver-signing enforcement or Windows security to install.
See VALIDATION.txt for exactly what was tested and its limits.
VB-Audio licensing/credit: see THIRD-PARTY.txt.
