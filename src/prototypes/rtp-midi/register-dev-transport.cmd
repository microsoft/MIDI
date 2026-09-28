@echo off
rem PROTOTYPE. Installs the rtpMIDI transport plugin into a local Windows MIDI Services
rem install, for development. There is no installer for it.
rem
rem Needs an administrator prompt, and Windows developer mode, because the plugin is not signed
rem and the service only loads unsigned plugins in developer mode.
rem Build src\in-box\Transport\RtpMidiTransport (Release, x64) first. Undo with unregister-dev-transport.cmd.
rem Once registered, build\replace_just_rtpmidi_x64.bat is enough to deploy a new build.

echo This must be run as administrator, with developer mode on.

set servicepath="%ProgramFiles%\Windows MIDI Services\Service"
set buildoutput="%~dp0..\..\in-box\VSFiles\x64\Release"

if not exist %buildoutput%\Midi2.RtpMidiTransport.dll (
    echo ERROR: %buildoutput%\Midi2.RtpMidiTransport.dll is missing. Build the transport first.
    pause
    exit /b 1
)

echo Stopping midisrv
net stop midisrv

rem SCM can report the service stopped while the process still holds the DLL, so the copy is
rem retried rather than the process being polled
echo Copying the rtpMIDI transport
for /L %%i in (1,1,30) do (
    copy /Y %buildoutput%\Midi2.RtpMidiTransport.dll %servicepath% >nul 2>&1
    if not errorlevel 1 goto :dll_copied
    echo   the installed binary is still locked, retrying...
    timeout /t 1 /nobreak >nul
)

echo.
echo ERROR: the transport DLL could not be replaced. Something still has it open:
echo.
tasklist /FI "IMAGENAME eq midisrv.exe"
echo.
pause
exit /b 1

:dll_copied
echo   transport DLL copied.
copy /Y %buildoutput%\Midi2.RtpMidiTransport.pdb %servicepath% >nul 2>&1

echo Registering the COM server
regsvr32 /s "%ProgramFiles%\Windows MIDI Services\Service\Midi2.RtpMidiTransport.dll"

echo Registering the transport plugin
rem No explicit ACL: the key must stay readable by the LOCAL SERVICE account midisrv runs as,
rem and it inherits exactly that from the parent.
reg add "HKLM\SOFTWARE\Microsoft\Windows MIDI Services\Transport Plugins\Midi2RtpMidiTransport" /v CLSID /t REG_SZ /d "{54c9b2f6-c235-4000-a675-9f6958a1a4fa}" /f
reg add "HKLM\SOFTWARE\Microsoft\Windows MIDI Services\Transport Plugins\Midi2RtpMidiTransport" /v Enabled /t REG_DWORD /d 1 /f

net start midisrv

pause
