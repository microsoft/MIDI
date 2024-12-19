@echo off
echo This is for copying build artifacts into the system for testing. 
echo This will only replace the github-built Windows service and components
echo If you are using the in-box service or dism-deployed service, this will not work.
echo This must be run as administrator.

set servicepath="%ProgramFiles%\Windows MIDI Services\Service"
set apipath="%ProgramFiles%\Windows MIDI Services\API"
set dmppath="%ProgramFiles%\Windows MIDI Services\"
set buildoutput="%midi_repo_root%src\api\VSFiles\x64\Release"

echo Stopping midisrv
net stop midisrv

echo stopping AEB
net stop /Y AudioEndpointBuilder

timeout 3
%midi_repo_root%build\sfpcopy %buildoutput%\wdmaud2.drv %windir%\system32\wdmaud2.drv

net start audiosrv

pause