@echo off
echo This is for copying build artifacts into the system for testing. 
echo This will only replace the github-built Windows SDK
echo If you are using the in-box service or dism-deployed service, this will not work.
echo This must be run as administrator.

set sdkinstallpath="%ProgramFiles%\Windows MIDI Services\Desktop App SDK Runtime"
set toolsinstallpath="%ProgramFiles%\Windows MIDI Services\Tools"

set sdkbuildoutput="%midi_repo_root%src\app-sdk\VSFiles\out\Microsoft.Windows.Devices.Midi2\x64\Debug"
set diagbuildoutput="%midi_repo_root%src\app-sdk\VSFiles\out\mididiag\x64\Debug"

echo Copying SDK files : dll
copy /Y %sdkbuildoutput%\Microsoft.Windows.Devices.Midi2.dll %sdkinstallpath%
echo Copying SDK files : pri
copy /Y %sdkbuildoutput%\Microsoft.Windows.Devices.Midi2.pri %sdkinstallpath%
echo Copying SDK files : winmd
copy /Y %sdkbuildoutput%\Microsoft.Windows.Devices.Midi2.winmd %sdkinstallpath%
echo Copying SDK files : pdb
copy /Y %sdkbuildoutput%\Microsoft.Windows.Devices.Midi2.pdb %sdkinstallpath%

echo Registering COM entry point into SDK dll
regsvr32 /s %sdkinstallpath%\Microsoft.Windows.Devices.Midi2.dll

echo Copying SDK files : mididiag.exe
copy /Y %diagbuildoutput%\mididiag.exe %toolsinstallpath%

pause