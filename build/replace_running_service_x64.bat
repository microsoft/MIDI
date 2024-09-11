@echo off
echo This is for copying build artifacts into the system for testing. 
echo This will only replace the github-built Windows service and components
echo If you are using the in-box service or dism-deployed service, this will not work.
echo This must be run as administrator.

set servicepath="%ProgramFiles%\Windows MIDI Services\Service"
set apipath="%ProgramFiles%\Windows MIDI Services\API"
set dmppath="%ProgramFiles%\Windows MIDI Services\"
set buildoutput="%midi_repo_root%src\api\VSFiles\x64\Release"



REM echo Stopping midisrv so we can replace it
REM sc stop midisrv


echo Uninstalling midisrv (if present)
%servicepath%\midisrv.exe uninstall

echo stopping AEB
net stop /Y AudioEndpointBuilder

timeout 3
echo Copying MidiSrv.exe and related dlls
copy /Y %buildoutput%\MidiSrv.exe %servicepath%
copy /Y %buildoutput%\Midi2.*Abstraction.dll %servicepath%
copy /Y %buildoutput%\Midi2.*Transform.dll %servicepath%
REM copy /Y %buildoutput%\wdmaud2.drv %windir%\system32
%midi_repo_root%build\sfpcopy %buildoutput%\wdmaud2.drv %windir%\system32\wdmaud2.drv



REM echo mididmp.exe
REM copy /Y %buildoutput%\mididmp.exe %dmppath%

REM echo API impl
REM copy /Y %buildoutput%\*.Devices.Midi2.dll %apipath%
REM echo API pri
REM copy /Y %buildoutput%\*.Devices.Midi2.pri %apipath%


echo Reinstalling service
%servicepath%\midisrv.exe install

net start audiosrv

pause