:: Convenience wrapper so the service plugins build can be started from a plain cmd prompt.
:: Everything lives in build-plugins.ps1; see .\build-plugins.ps1 -? for options.
@ECHO OFF
pwsh -ExecutionPolicy Bypass -NoProfile -File "%~dp0build-plugins.ps1" %*
IF ERRORLEVEL 1 EXIT /B %ERRORLEVEL%
