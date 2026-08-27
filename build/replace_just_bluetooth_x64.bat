@echo off
echo This must be run as administrator.

rem The Bluetooth LE MIDI transport is preview code with no installer of its own yet, so this
rem also does the COM and transport plugin registration that an installer would normally do.

set servicepath="%ProgramFiles%\Windows MIDI Services\Service"
set buildoutput="%midi_repo_root%src\api\VSFiles\x64\Release"

echo Stopping midisrv
net stop midisrv

rem The transport has been called Midi2.Ble1MidiTransport, Midi2.Ble2MidiTransport and
rem Midi2.BleMidiTransport, all sharing this CLSID. Copying the new name leaves any old file
rem behind still registered, so they are unregistered FIRST: doing it after would tear down the
rem registration the new DLL had just written.
for %%o in (Midi2.Ble1MidiTransport Midi2.Ble2MidiTransport Midi2.BleMidiTransport) do (
    if exist "%ProgramFiles%\Windows MIDI Services\Service\%%o.dll" (
        echo Removing the old %%o
        regsvr32 /s /u "%ProgramFiles%\Windows MIDI Services\Service\%%o.dll"
        del /F /Q "%ProgramFiles%\Windows MIDI Services\Service\%%o.dll" >nul 2>&1
        del /F /Q "%ProgramFiles%\Windows MIDI Services\Service\%%o.pdb" >nul 2>&1
    )
)

rem SCM can report the service stopped while the process is still alive holding the DLL open, so
rem the copy is retried rather than the process being polled. Whether the file can be replaced is
rem the only thing that actually matters, and unlike a process check it cannot be fooled.
echo Copying Bluetooth LE MIDI Transport
for /L %%i in (1,1,30) do (
    copy /Y %buildoutput%\Midi2.BluetoothMidiTransport.dll %servicepath% >nul 2>&1
    if not errorlevel 1 goto :dll_copied
    echo   the installed binary is still locked, retrying...
    timeout /t 1 /nobreak >nul
)

echo.
echo ERROR: the transport DLL could not be replaced. The OLD build is still installed.
echo Something still has it open:
echo.
tasklist /FI "IMAGENAME eq midisrv.exe"
echo.
pause
exit /b 1

:dll_copied
echo   transport DLL replaced.
copy /Y %buildoutput%\Midi2.BluetoothMidiTransport.pdb %servicepath% >nul 2>&1

echo Registering the COM server
regsvr32 /s "%ProgramFiles%\Windows MIDI Services\Service\Midi2.BluetoothMidiTransport.dll"

echo Registering the transport plugin
rem No explicit ACL: the key must stay readable by the LOCAL SERVICE account midisrv runs as,
rem and it inherits exactly that from the parent.
reg add "HKLM\SOFTWARE\Microsoft\Windows MIDI Services\Transport Plugins\Midi2BluetoothMidiTransport" /v CLSID /t REG_SZ /d "{5dc87270-f318-4838-a4f9-6aadc63e925f}" /f
reg add "HKLM\SOFTWARE\Microsoft\Windows MIDI Services\Transport Plugins\Midi2BluetoothMidiTransport" /v Enabled /t REG_DWORD /d 1 /f

net start midisrv

pause
