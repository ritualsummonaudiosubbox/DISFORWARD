DISFORWARD 0.5 - Windows 11 x64

ONE-TIME SETUP
1. Extract the complete download into a folder on your PC.
2. Open DISFORWARD-Setup.exe and approve the administrator prompt.
3. In the original VB-CABLE installer, click Install Driver.
4. Restart Windows after installation, as VB-Audio requires.
   If VB-CABLE is already installed, skip installation and continue below.

BEFORE YOUR FIRST CALL
1. Open DISFORWARD.exe. Click SETUP, then Refresh.
2. Select your physical microphone under Your microphone.
3. Click Check audio line while you are outside a live call.
4. In the CALL APP, choose CABLE Output (VB-Audio Virtual Cable) as its ONE
   microphone input. If the cable was renamed previously, use the exact
   name displayed under Call app microphone input in DISFORWARD Setup.
5. Keep Windows, Discord and call-app PLAYBACK on your headphones.
   In Discord, keep INPUT on your own real microphone.
6. Make a short test call before customer use.

DAILY USE
My Mic          Your own microphone goes to the call app.
Discord Friend  Desktop Discord playback goes to the call app.
Mute            DISFORWARD releases the selected source after a short fade.
PIN             Keeps the compact console above normal/borderless windows.
SETUP           Microphone choice, line check, install, Refresh, sound settings.

The application starts muted. Choose only one source at a time.
You can hear your Discord friend in your headphones while they are forwarded.

IMPORTANT AUDIO SELECTIONS
Call-app microphone: CABLE Output (or the name displayed by DISFORWARD Setup).
Discord output:      Headphones. Do NOT choose CABLE Input.
Discord input:       Your real microphone. Do NOT choose CABLE Output.
Windows speakers:    Headphones, including default communications playback.

CABLE Input is the playback side DISFORWARD writes into.
CABLE Output is the microphone side your call app receives.
Do not send other apps into CABLE Input or enable Windows 'Listen to this
 device' on the cable. Those routes can bypass DISFORWARD's Mute/source switch.
The app checks whether the cable is your default playback device and asks
for headphones to prevent accidental desktop/call audio feedback.

DISCORD
Use the desktop Discord app on Windows 11. Open it and join your friend's call,
then press Discord Friend. If several Discord installations are running,
close the extra ones first. Browser Discord is not selected by this version.
This captures ALL Discord playback: calls, notifications, streams and
soundboards. Use a private call and disable unwanted Discord sounds.
No Discord login, account token or bot is required.

IF SETUP OR AUDIO FAILS
- If the vendor installer offers Remove Driver, do not keep reinstalling it.
  Restart first and click Refresh. For an intentionally clean reinstall,
  follow the vendor's uninstall -> restart -> install -> restart procedure.
- Confirm microphone privacy allows desktop apps in Windows Settings.
- Set VB-CABLE levels above zero and ensure it is unmuted in Windows Sound.
- Close exclusive-mode audio programs if a device is reported in use.
- Check audio line verifies a quiet tone returns through the cable; it does
  not verify Discord, your real microphone, or the remote call app.
- If Windows blocks the signed vendor driver, use the official VB-Audio
  support instructions. DISFORWARD never changes signing or Secure Boot.

QUALITY AND COMPATIBILITY
48 kHz stereo float32 WASAPI shared-mode routing, quality sample-rate
conversion, short source fades and bounded buffers. Source switching can
have a brief pause while Windows opens the selected source. No lossy codec
is added by DISFORWARD. Incoming Discord/microphone quality is preserved
as received; audio already compressed by Discord cannot be restored.
PIN cannot guarantee an overlay above exclusive fullscreen or secure desktop.

VB-CABLE is by VB-Audio: https://www.vb-cable.com/
It is donationware; donations are welcome. For call-center/business use,
follow the vendor's professional licensing terms:
https://vb-audio.com/Services/licensing.htm
See THIRD-PARTY.txt and VALIDATION.txt.
