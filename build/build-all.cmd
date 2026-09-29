:: Convenience wrapper so the full Windows MIDI Services build can be started from a plain cmd prompt.
:: Everything lives in build-all.ps1; see .\build-all.ps1 -? for options.
@ECHO OFF
pwsh -ExecutionPolicy Bypass -NoProfile -File "%~dp0build-all.ps1" %*
IF ERRORLEVEL 1 EXIT /B %ERRORLEVEL%
