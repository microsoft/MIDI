@echo off
echo This must be run as administrator.

set servicepath="%ProgramFiles%\Windows MIDI Services\Service"
set buildoutput="%midi_repo_root%src\in-box\VSFiles\x64\Release"

echo Stopping midisrv
net stop midisrv

echo Copying GM Synth Transport
copy /Y %buildoutput%\Midi2.MidiSynthTransport.dll %servicepath%

echo Registering the COM server
regsvr32 /s %servicepath%\Midi2.MidiSynthTransport.dll

echo Registering the transport plugin
reg add "HKLM\SOFTWARE\Microsoft\Windows MIDI Services\Transport Plugins\Midi2MidiSynthTransport" /v CLSID /t REG_SZ /d "{7605713E-FEA9-409D-A90F-A81233200D0A}" /f
reg add "HKLM\SOFTWARE\Microsoft\Windows MIDI Services\Transport Plugins\Midi2MidiSynthTransport" /v Enabled /t REG_DWORD /d 1 /f

net start midisrv

pause
