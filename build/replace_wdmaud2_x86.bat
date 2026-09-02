@echo off
echo This is for copying build artifacts into the system for testing. 
echo This will only replace the github-built Windows service and components
echo If you are using the in-box service or dism-deployed service, this will not work.
echo This must be run as administrator.
echo.
echo This is the 32-bit driver used by 32-bit winmm apps like MIDI-OX. It is a separate
echo binary from the one in System32, so replacing that one does not cover these apps.

set buildoutput="%midi_repo_root%src\in-box\VSFiles\Win32\Release"

echo.
echo Close any running 32-bit MIDI apps first. midisrv does not load this file, so
echo stopping the service does not release it - the apps holding it do.
pause

%midi_repo_root%build\sfpcopy %buildoutput%\wdmaud2.drv %windir%\SysWOW64\wdmaud2.drv

pause
