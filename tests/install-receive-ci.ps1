# Disposable Windows runner only. Uses the original public vendor installer.
$ErrorActionPreference='Stop'
$archive=Join-Path $pwd 'third_party/HiFiCableAsioBridgeSetup_v1007.zip'
Invoke-WebRequest https://download.vb-audio.com/Download_CABLE/HiFiCableAsioBridgeSetup_v1007.zip -OutFile $archive -TimeoutSec 60
if ((Get-FileHash $archive).Hash -ne '3ecf204bfd8579d36bb918f9856eb1eaddd49c75146f5e8a8f59dcb8375ae89a') { throw 'Receive installer checksum failed' }
$extract=Join-Path $pwd 'third_party/HiFi'
Expand-Archive $archive $extract -Force
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class VendorUi {
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
}
'@
$p=Start-Process (Join-Path $extract HiFiCableAsioBridgeSetup.exe) -PassThru
try {
 for ($i=0;$i -lt 60;$i++) { $p.Refresh();if ($p.MainWindowHandle -ne [IntPtr]::Zero) { break };Start-Sleep -Milliseconds 100 }
 $h=$p.MainWindowHandle
 if ($h -eq [IntPtr]::Zero) { throw 'Original receive installer window missing' }
 # Install button visually verified in the original 900 x 554 installer window.
 [void][VendorUi]::PostMessage($h,0x201,[IntPtr]1,[IntPtr]((475 -shl 16) -bor 730))
 [void][VendorUi]::PostMessage($h,0x202,[IntPtr]::Zero,[IntPtr]((475 -shl 16) -bor 730))
 $roots=@('C:/Program Files/VB','C:/Program Files (x86)/VB','C:/Program Files/VB-Audio','C:/Program Files (x86)/VB-Audio')
 $inf=$null
 for ($i=0;$i -lt 60;$i++) {
  foreach ($root in $roots) { if(Test-Path $root) { $inf=Get-ChildItem $root -Filter VBhfvaio64_win7.inf -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1;if($inf){break} } }
  if($inf){break};Start-Sleep -Milliseconds 500
 }
 if(-not $inf){throw 'Original vendor installer did not extract receive INF'}
 Write-Host "Original receive INF: $($inf.FullName)"
 $cat=Join-Path $inf.DirectoryName vbaudio_hfvaio64_win7.cat
 $sys=Join-Path $inf.DirectoryName vbaudio_hfvaio64_win7.sys
 signtool verify /kp /v /c $cat $sys
 if($LASTEXITCODE){throw 'Receive driver production signature verification failed'}
 # Equivalent to consenting to this genuine vendor publisher in the Windows
 # installation dialog, on this disposable CI VM only. Never install a root.
 $signature=Get-AuthenticodeSignature $cat
 if($signature.SignerCertificate.Thumbprint -ne '00859AAC6A54B8C1B3C139DE67846E64E7B82DB2'){throw 'Unexpected receive publisher certificate'}
 $store=[Security.Cryptography.X509Certificates.X509Store]::new('TrustedPublisher','LocalMachine')
 try{$store.Open('ReadWrite');$store.Add($signature.SignerCertificate)}finally{$store.Close()}
 ./install-vbcable-ci.exe $inf.FullName VBAudioHFVAIO
 if($LASTEXITCODE){throw 'Receive driver installation failed'}
} finally { if(-not $p.HasExited){$p.Kill()} }
