@echo off
echo This is for copying build artifacts into the system for testing. 
echo This will only replace the github-built Windows SDK
echo If you are using the in-box service or dism-deployed service, this will not work.
echo This must be run as administrator.

set sdkinstallpath="%ProgramFiles%\Windows MIDI Services\Desktop App SDK Runtime"
set toolsinstallpath="%ProgramFiles%\Windows MIDI Services\Tools"

set sdkbuildoutput="d:\DVE\Projects\C++\MIDI\src\app-sdk\vsfiles\out\Microsoft.Windows.Devices.Midi2\x64\Release"
set diagbuildoutput="d:\DVE\Projects\C++\MIDI\src\app-sdk\vsfiles\out\mididiag\x64\Release"

echo Copying SDK files : dll
copy /Y %sdkbuildoutput%\Microsoft.Windows.Devices.Midi2.dll %sdkinstallpath%
echo Copying SDK files : pri
copy /Y %sdkbuildoutput%\Microsoft.Windows.Devices.Midi2.pri %sdkinstallpath%
echo Copying SDK files : winmd
copy /Y %sdkbuildoutput%\Microsoft.Windows.Devices.Midi2.winmd %sdkinstallpath%

echo Registering COM entry point into SDK dll
regsvr32 /s %sdkinstallpath%\Microsoft.Windows.Devices.Midi2.dll

echo Copying SDK files : mididiag.exe
copy /Y %diagbuildoutput%\mididiag.exe %toolsinstallpath%

pause