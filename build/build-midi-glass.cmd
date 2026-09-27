:: Convenience wrapper so the MIDI Glass installer build can be started from a plain cmd prompt.
:: Everything lives in build-midi-glass.ps1; see .\build-midi-glass.ps1 -? for options.
@ECHO OFF
pwsh -ExecutionPolicy Bypass -NoProfile -File "%~dp0build-midi-glass.ps1" %*
IF ERRORLEVEL 1 EXIT /B %ERRORLEVEL%
