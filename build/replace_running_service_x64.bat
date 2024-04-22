@echo off
echo This is for copying build artifacts into the system for testing. 
echo This will only replace the github-built Windows service and components
echo If you are using the in-box service or dism-deployed service, this will not work.
echo This must be run as administrator.

sc stop midisrv

set servicepath="%ProgramFiles%\Windows MIDI Services\Service"
set apipath="%ProgramFiles%\Windows MIDI Services\API"
set dmppath="%ProgramFiles%\Windows MIDI Services\"
set buildoutput="%midi_repo_root%src\api\VSFiles\x64\Release"

echo mididmp.exe
copy /Y %buildoutput%\mididmp.exe %dmppath%

echo MidiSrv.exe
copy /Y %buildoutput%\MidiSrv.exe %servicepath%
copy /Y %buildoutput%\Midi2.*Abstraction.dll %servicepath%
copy /Y %buildoutput%\Midi2.*Transform.dll %servicepath%

echo Windows.Devices.Midi2.dll
copy /Y %buildoutput%\*.Devices.Midi2.dll %apipath%
echo Windows.Devices.Midi2.pri
copy /Y %buildoutput%\*.Devices.Midi2.pri %apipath%



sc start midisrv

pause