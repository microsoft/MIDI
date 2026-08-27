@echo off
REM Runs the Bluetooth MIDI transport unit tests. These need no radio, no device and no service.
REM A .cmd because PowerShell mangles the /name: filter argument.

setlocal

set TESTDLL=%~dp0..\..\VSFiles\x64\Release\Midi2.Transport.BleMidi.unittests.dll
set TE="%WindowsSdkDir%Testing\Runtimes\TAEF\x64\TE.exe"

if not exist %TE% set TE="C:\Program Files (x86)\Windows Kits\10\Testing\Runtimes\TAEF\x64\TE.exe"

%TE% "%TESTDLL%" %*

endlocal
