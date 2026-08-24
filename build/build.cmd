:: Convenience wrapper so the release build can be started from a plain cmd prompt.
:: Everything lives in build.ps1; see .\build.ps1 -? for options.
@ECHO OFF
pwsh -ExecutionPolicy Bypass -NoProfile -File "%~dp0build.ps1" %*
IF ERRORLEVEL 1 EXIT /B %ERRORLEVEL%
