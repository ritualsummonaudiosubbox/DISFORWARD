# DISFORWARD

Compact Windows x64 console for forwarding your mic or Discord audio into one
call-app microphone input. Version 0.6.3 adds an independent Discord receive line
and **SPEAKER** monitoring. Windows default playback/communications settings
are neither changed nor used to select the routes.

| Application setting | Device |
| --- | --- |
| Discord output | Speakers (VB-Audio Hi-Fi Cable) |
| Discord input | Your physical microphone |
| Call app microphone | CABLE Output (VB-Audio Virtual Cable) |
| Call app playback | Your normal headphones/speakers |
| DISFORWARD SPEAKER | Explicit headphones/speakers selection in Setup |

The receive line can also appear as **Hi-Fi Cable Input (VB-Audio Hi-Fi Cable)**.
Setup displays the actual Windows name. Vendor device names and driver identities
are preserved. Two independent vendor drivers are required: basic VB-CABLE for
sending, Hi-Fi Cable for receiving. The basic cable's stereo/16-channel
endpoints share one bus and are not used as two independent cables.

## Setup and controls

Extract the complete download. Run `DISFORWARD-Setup.exe`; skip SEND if CABLE
Output is already installed. The receive installer downloads the original
Hi-Fi Cable package directly from VB-Audio, verifies its checksum and opens the
vendor installer. Restart Windows, open DISFORWARD, Setup, Refresh, then select
your mic and listening device. Run **Check audio lines** before a live call.

DISFORWARD negotiates both receive and send cable engines at 48 kHz using the documented
Windows stream-format API and keeps them active. It does not rewrite the
persistent endpoint formats. If another client locks the format, close Discord
playback/ASIO Bridge and Refresh; setting both endpoint defaults to 48 kHz is a
fallback. ASIO Bridge and Windows "Listen to this device" should be off.
Follow [START-HERE.txt](START-HERE.txt) for the complete setup.

- **My Mic / Discord Friend / Mute** choose what the phone call hears.
- **SPEAKER** hears Discord independently, including while the phone is muted.
- **PIN** keeps the 360 × 208 console topmost over normal/borderless windows.
  Windows exclusive fullscreen and secure desktops can cover it.

Only the Discord receive line reaches SPEAKER. Your microphone and the phone
send bus are never selected as monitor inputs. Keep Discord input on the
physical mic to prevent digital feedback. Headphones prevent speaker sound
from re-entering a nearby microphone; acoustic echo cannot be ruled out when
using physical speakers. Discord's selected output includes its call and other
sounds, not one isolated named participant in a group call.

## Build and verification

[Windows builds](https://github.com/ritualsummonaudiosubbox/DISFORWARD/actions/workflows/windows.yml)

Use x64 Visual Studio Developer PowerShell with Desktop C++ and Windows SDK:

```powershell
./build-windows.ps1
```

The build verifies the pinned basic vendor archive, builds both EXEs with
MSVC `/W4 /WX`, and packages the guides into
`artifacts/DISFORWARD-Windows-x64`. No WDK is needed. App/setup EXEs are unsigned;
the original vendor kernel packages are used without binary modification.

CI checks audio FIFO/fades, distinct cable identities, UI controls and kernel
catalog signatures. Its disposable Windows runner installs both cables and
checks real receive/send tone routing, muted-bus isolation, separate engine
lifetimes and unchanged default endpoints. The listening lifecycle test uses
a virtual test sink because the runner has no physical speakers. The packaged
`WINDOWS-AUDIO-CHECK.txt` records the actual result. Real Discord, a physical
headset and the customer phone app still require a live acceptance call.

Processing uses 48 kHz stereo float32 WASAPI with quality conversion, short
source fades and clock-drift correction. No recording, network audio upload,
automatic gain boost or voice filtering is performed.

[VB-Audio licensing](https://vb-audio.com/Services/licensing.htm) applies
separately, including professional/distributor terms. See THIRD-PARTY.txt.
Earlier custom-driver source remains in `driver/` for future development;
it is not built or installed by this package.
