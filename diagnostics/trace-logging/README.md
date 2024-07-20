# MIDI Trace Logging

The .wprp file here is configured to record the data needed to diagnose issues with Windows MIDI Services. If you have a crash or other issue, this is one of the best ways to get the information to us.

## How to use

Download these files (download raw files) from Github. 

1. Open an administrator command prompt (do not double-click these batch files, you must open an administrator cmd window).
2. `CD` to the folder with the wprp file and the batch files. The folder with the wprp file must be the current folder. For example `cd G:\GitHub\microsoft\midi\diagnostics\trace-logging`
3. Run `trace.bat` in that same administrator command window (the folder with the wprp file must be the current folder). This runs the two batch files explained below.
4. Recreate the issue
5. Hit enter in the command window running the batch file

Alternate approach: 

1. Open an administrator command prompt (do not double-click these batch files, you must open an administrator cmd window)
2. `CD` to the folder with the wprp file and the batch files. The folder with the wprp file must be the current folder. For example `cd G:\GitHub\microsoft\midi\diagnostics\trace-logging`
3. Run `start_capture.bat` as administrator. This will start recording Windows MIDI Services activity on the system. We record related API calls, errors, etc. No screenshots are taken.
4. Recreate the issue
5. Run `save_capture.bat` as administrator to stop recording and create the `.etl` file.
6. Provide the `.etl` file to us in a secure way. These tend to be large, so OneDrive or Google Drive or other similar sharing services are preferred.

This is all very similar to what Feedback Hub does when you record a repro problem. 

## Troubleshooting

If you receive "DTD is prohibited" it's likely you did not get the raw version of the wprp file from github. The file should be straight XML, without any HTML elements in it.

## Looking at the output yourself

If you want to look inside the `.etl` file, [install Windows Performance Analyzer](https://learn.microsoft.com/windows-hardware/test/wpt/windows-performance-analyzer) and then open the file. Double-click the System Activity on the left and you'll be able to see the events that we log in the various components. Not all events are logged, of course, but we do log key ones.