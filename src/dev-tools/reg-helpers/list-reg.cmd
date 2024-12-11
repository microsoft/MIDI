@echo off
echo[
echo ==================================================================================
echo To run this script, you must have the ability to run local scripts turned on. You 
echo can find this in Settings - System - For Developers - Powershell
echo[
echo You also need to run this from an elevated command prompt. Do not just double
echo click this cmd file.
echo ==================================================================================
echo[

pwsh -Command Unblock-File .\midi-list-reg.ps1

pwsh .\midi-list-reg.ps1