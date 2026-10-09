@echo off
REM Runs the RTP-MIDI transport unit tests. They need no MIDI service and no admin rights: the
REM transport DLL is loaded into the test process, and remote devices talk to it over loopback.
REM MidiDnssdAdvertiserTests register a made-up service with the Windows DNS client, which
REM announces it on the local network until the test withdraws it.
REM A .cmd because PowerShell mangles the /name: filter argument.

setlocal

set TESTDLL=%~dp0..\..\VSFiles\x64\Release\Midi2.Transport.RtpMidi.unittests.dll
set TE="%WindowsSdkDir%Testing\Runtimes\TAEF\x64\TE.exe"

if not exist %TE% set TE="C:\Program Files (x86)\Windows Kits\10\Testing\Runtimes\TAEF\x64\TE.exe"

%TE% "%TESTDLL%" %*

endlocal
