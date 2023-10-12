REM Copyright (c) Microsoft Corporation. All rights reserved.
@echo off
setlocal

set script=%~dp0CollectMidiLogs.ps1

rem look out for WOW64 file system redirection
set powershell="%windir%\system32\WindowsPowerShell\v1.0\PowerShell.exe"
if not "%PROCESSOR_ARCHITEW6432%" == "" (
    set powershell="%windir%\sysnative\WindowsPowerShell\v1.0\PowerShell.exe"
)

set command= ^
    Start-Process powershell.exe ^
        -Verb RunAs ^
        -ArgumentList ' ^
            -NoProfile ^
            -NoExit ^
            -ExecutionPolicy Bypass ^
            -File ""%script%"" ^
        '

call %powershell% -NoProfile -Command "& { %command% }"
