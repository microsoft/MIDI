@echo off
rem PROTOTYPE. Removes what register-dev-transport.cmd installed. Needs an administrator prompt.

echo This must be run as administrator.

set servicedll="%ProgramFiles%\Windows MIDI Services\Service\Midi2.RtpMidiTransport.dll"

echo Stopping midisrv
net stop midisrv

echo Removing the transport plugin registration
reg delete "HKLM\SOFTWARE\Microsoft\Windows MIDI Services\Transport Plugins\Midi2RtpMidiTransport" /f

if exist %servicedll% (
    echo Unregistering the COM server
    regsvr32 /s /u %servicedll%

    for /L %%i in (1,1,30) do (
        del /F /Q %servicedll% >nul 2>&1
        if not exist %servicedll% goto :deleted
        echo   the installed binary is still locked, retrying...
        timeout /t 1 /nobreak >nul
    )

    echo ERROR: %servicedll% could not be deleted.
    goto :restart
)

:deleted
del /F /Q "%ProgramFiles%\Windows MIDI Services\Service\Midi2.RtpMidiTransport.pdb" >nul 2>&1

:restart
net start midisrv

pause
