@echo off
echo This must be run as administrator.

set servicepath="%ProgramFiles%\Windows MIDI Services\Service"
set apipath="%ProgramFiles%\Windows MIDI Services\API"
set dmppath="%ProgramFiles%\Windows MIDI Services\"
set buildoutput="%midi_repo_root%src\api\VSFiles\x64\Release"



echo Stopping midisrv
net stop midisrv

echo Copying Network MIDI Transport
copy /Y %buildoutput%\Midi2.NetworkMidiTransport.dll %servicepath%

net start midisrv

pause