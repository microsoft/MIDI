@echo off
echo This must be run as administrator.

rem The Bluetooth LE MIDI transport is preview code with no installer of its own yet, so this
rem also does the COM and transport plugin registration that an installer would normally do.

set servicepath="%ProgramFiles%\Windows MIDI Services\Service"
set buildoutput="%midi_repo_root%src\api\VSFiles\x64\Release"

echo Stopping midisrv
net stop midisrv

echo Copying Bluetooth LE MIDI Transport
copy /Y %buildoutput%\Midi2.Ble2MidiTransport.dll %servicepath%
copy /Y %buildoutput%\Midi2.Ble2MidiTransport.pdb %servicepath%

echo Registering the COM server
regsvr32 /s "%ProgramFiles%\Windows MIDI Services\Service\Midi2.Ble2MidiTransport.dll"

echo Registering the transport plugin
rem No explicit ACL: the key must stay readable by the LOCAL SERVICE account midisrv runs as,
rem and it inherits exactly that from the parent.
reg add "HKLM\SOFTWARE\Microsoft\Windows MIDI Services\Transport Plugins\Midi2BluetoothMidiTransport" /v CLSID /t REG_SZ /d "{5dc87270-f318-4838-a4f9-6aadc63e925f}" /f
reg add "HKLM\SOFTWARE\Microsoft\Windows MIDI Services\Transport Plugins\Midi2BluetoothMidiTransport" /v Enabled /t REG_DWORD /d 1 /f

net start midisrv

pause
