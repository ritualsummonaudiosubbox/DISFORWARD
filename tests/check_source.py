from pathlib import Path
import re,xml.etree.ElementTree as ET
root=Path(__file__).resolve().parents[1]
inf=(root/'driver/Source/Main/DisforwardAudio.inx').read_text(encoding='ascii')
assert 'ROOT\\DISFORWARD' in inf and 'ROOT\\SimpleAudioSample' not in inf
assert inf.count('DISFORWARD.Native.1')==2
assert 'CatalogFile = DisforwardAudio.cat' in inf
assert 'ServiceBinary=%13%\\disforwardaudio.sys' in inf
stream=(root/'driver/Source/Main/minwavertstream.cpp').read_text()
for method in ['AllocateBufferWithNotification','AllocateAudioBuffer','SetState','SetFormat','FreeAudioBuffer','GetPosition','NonDelegatingQueryInterface','UpdatePosition','WriteBytes','ReadBytes']:
 assert 'CMiniportWaveRTStream::'+method in stream,method
assert 'm_ToneGenerator.GenerateSine' not in stream
assert 'm_SaveData.WriteData' not in stream
assert 'DisforwardWrite(m_pDmaBuffer' in stream and 'DisforwardRead(m_pDmaBuffer' in stream
for project in (root/'driver').rglob('*.vcxproj'):ET.parse(project)
for project in (root/'driver').rglob('*.VcxProj'):ET.parse(project)
app=(root/'app/console.cpp').read_text()
assert 'e.identity==L"DISFORWARD.Native.1"' in app
assert not list(root.rglob('*VBCABLE*'))
assert not list(root.rglob('*.sys')),'No kernel binary may be claimed without a Windows build.'
print('PASS: native identity, INF/project XML, retained stream methods, transport integration and absence of third-party driver binaries.')
