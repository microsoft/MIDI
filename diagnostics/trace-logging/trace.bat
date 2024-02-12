@echo off
echo Must be run from an administrator command prompt

wpr -start MidiServices.wprp -filemode

echo Now go recreate the issue. Hit enter when done.

pause

wpr -stop TraceCaptureFile.etl

start TraceCaptureFile.etl