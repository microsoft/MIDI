@echo off

rmdir /S build

cmake -S src -B build
if errorlevel 1 goto quit

cmake --build build
if errorlevel 1 goto quit


:quit