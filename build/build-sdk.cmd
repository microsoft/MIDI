:: Convenience wrapper so the App SDK release build can be started from a plain cmd prompt.
:: Everything lives in build-sdk.ps1; see .\build-sdk.ps1 -? for options.
@ECHO OFF
pwsh -ExecutionPolicy Bypass -NoProfile -File "%~dp0build-sdk.ps1" %*
IF ERRORLEVEL 1 EXIT /B %ERRORLEVEL%
