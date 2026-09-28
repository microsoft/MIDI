@echo off
echo This must be run as administrator.

set servicepath="%ProgramFiles%\Windows MIDI Services\Service"
set buildoutput="%midi_repo_root%src\in-box\VSFiles\x64\Release"



echo Stopping midisrv
net stop midisrv

echo Copying RTP-MIDI Transport
copy /Y %buildoutput%\Midi2.RtpMidiTransport.dll %servicepath%

net start midisrv

pause
