@echo off
echo This is for copying build artifacts into the system for testing. 
echo This will only replace the github-built Windows service and components
echo If you are using the in-box service or dism-deployed service, this will not work.
echo This must be run as administrator.

set servicepath="%ProgramFiles%\Windows MIDI Services\Service"
set apipath="%ProgramFiles%\Windows MIDI Services\API"
set dmppath="%ProgramFiles%\Windows MIDI Services\"
set buildoutput="%midi_repo_root%src\in-box\VSFiles\x64\Debug"



REM echo Stopping midisrv so we can replace it
REM sc stop midisrv


echo Uninstalling midisrv (if present)
%servicepath%\midisrv.exe uninstall

echo stopping AEB
net stop /Y AudioEndpointBuilder

timeout 3

rem The BLE transport has been renamed more than once and every old name shares the new one's
rem CLSID. The wildcard copy below brings in the current name but leaves any old file behind,
rem still registered.
for %%o in (Midi2.Ble1MidiTransport Midi2.Ble2MidiTransport Midi2.BleMidiTransport) do (
    if exist "%ProgramFiles%\Windows MIDI Services\Service\%%o.dll" (
        echo Removing the superseded %%o
        regsvr32 /s /u "%ProgramFiles%\Windows MIDI Services\Service\%%o.dll"
        del /F /Q "%ProgramFiles%\Windows MIDI Services\Service\%%o.dll" >nul 2>&1
        del /F /Q "%ProgramFiles%\Windows MIDI Services\Service\%%o.pdb" >nul 2>&1
    )
)

echo Copying MidiSrv.exe and related dlls
copy /Y %buildoutput%\MidiSrv.exe %servicepath%
copy /Y %buildoutput%\Midi2.*Transport.dll %servicepath%
copy /Y %buildoutput%\Midi2.*Transform.dll %servicepath%

REM %midi_repo_root%build\sfpcopy %buildoutput%\wdmaud2.drv %windir%\system32\wdmaud2.drv

echo Reinstalling service
%servicepath%\midisrv.exe install

net start audiosrv

pause