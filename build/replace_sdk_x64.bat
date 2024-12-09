@echo off
echo This is for copying build artifacts into the system for testing. 
echo This will only replace the github-built Windows SDK
echo If you are using the in-box service or dism-deployed service, this will not work.
echo This must be run as administrator.

set sdkinstallpath="%ProgramFiles%\Windows MIDI Services\Desktop App SDK Runtime"
set toolsinstallpath="%ProgramFiles%\Windows MIDI Services\Tools"

set sdkbuildoutput="%midi_repo_root%src\app-sdk\VSFiles\out\Microsoft.Windows.Devices.Midi2\x64\Release"
set diagbuildoutput="%midi_repo_root%src\app-sdk\VSFiles\out\mididiag\x64\Release"

echo Copying SDK files
copy /Y %sdkbuildoutput%\Microsoft.Windows.Devices.Midi2.dll %sdkinstallpath%
copy /Y %sdkbuildoutput%\Microsoft.Windows.Devices.Midi2.pri %sdkinstallpath%
copy /Y %sdkbuildoutput%\Microsoft.Windows.Devices.Midi2.winmd %sdkinstallpath%

regsvr32 %sdkinstallpath%\Microsoft.Windows.Devices.Midi2.dll

copy /Y %diagbuildoutput%\mididiag.exe %toolsinstallpath%

pause