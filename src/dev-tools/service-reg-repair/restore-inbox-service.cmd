@echo off
echo[
echo ==================================================================================
echo To run this script, you must have the ability to run local scripts turned on. You 
echo can find this in Settings - System - For Developers - Powershell
echo[
echo You also need to run this from an elevated command prompt. Do not just double
echo click this cmd file.
echo[
echo This must be run from a normal Windows command shell. If you are running
echo PowerShell as your shell, please run the individual commands:
echo     Unblock-File .\midi-list-reg.ps1
echo     .\midi-list-reg.ps1
echo ==================================================================================
echo[

pwsh -Command Unblock-File .\restore-inbox-service.ps1

pwsh .\restore-inbox-service.ps1