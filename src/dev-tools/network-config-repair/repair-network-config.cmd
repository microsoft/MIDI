@echo off
echo[
echo ==================================================================================
echo Windows MIDI Services : Network MIDI 2.0 configuration repair
echo[
echo To run this script, you must have the ability to run local scripts turned on. You
echo can find this in Settings - System - For Developers - Powershell
echo[
echo Run this from an elevated command prompt. Do not just double click this cmd file.
echo[
echo If you are already running PowerShell as your shell, run these instead:
echo     Unblock-File .\repair-network-config.ps1
echo     .\repair-network-config.ps1
echo ==================================================================================
echo[

powershell -NoProfile -Command Unblock-File .\repair-network-config.ps1

powershell -NoProfile -ExecutionPolicy Bypass -File .\repair-network-config.ps1 %*
