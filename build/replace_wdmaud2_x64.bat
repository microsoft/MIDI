@echo off
echo This is for copying build artifacts into the system for testing. 
echo This will only replace the github-built Windows service and components
echo If you are using the in-box service or dism-deployed service, this will not work.
echo This must be run as administrator.

set servicepath="%ProgramFiles%\Windows MIDI Services\Service"
set apipath="%ProgramFiles%\Windows MIDI Services\API"
set dmppath="%ProgramFiles%\Windows MIDI Services\"
set buildoutput=d:\DVE\Projects\C++\MIDI\src\api\VSFiles\x64\Release
set reporoot=d:\DVE\Projects\C++\MIDI

echo Stopping midisrv
net stop midisrv

echo stopping AEB
net stop /Y AudioEndpointBuilder

timeout 3
echo Taking ownership of wdmaud2.drv
takeown /f %windir%\system32\wdmaud2.drv
echo Granting permissions
icacls %windir%\system32\wdmaud2.drv /grant administrators:F

echo Stopping Windows Audio service
net stop audiosrv

echo Copying wdmaud2.drv to system32
copy /Y "%buildoutput%\wdmaud2.drv" %windir%\system32\wdmaud2.drv
if errorlevel 1 (
    echo Failed to copy file. Trying alternative method...
    powershell -ExecutionPolicy Bypass -Command "& '%reporoot%\build\sfpcopy.ps1' -Source '%buildoutput%\wdmaud2.drv' -Destination '%windir%\system32\wdmaud2.drv'"
)

net start audiosrv

pause