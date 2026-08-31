:: Convenience wrapper so the Bluetooth MIDI preview build can be started from a plain cmd prompt.
:: Everything lives in build-bluetooth.ps1; see .\build-bluetooth.ps1 -? for options.
@ECHO OFF
pwsh -ExecutionPolicy Bypass -NoProfile -File "%~dp0build-bluetooth.ps1" %*
IF ERRORLEVEL 1 EXIT /B %ERRORLEVEL%
