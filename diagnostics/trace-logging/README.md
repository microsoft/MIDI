# MIDI Trace Logging

The .wprp file here is configured to record the data needed to diagnose issues with Windows MIDI Services. If you have a crash or other issue, this is one of the best ways to get the information to us.

## How to use

Open an administrator command prompt and CD to the folder containing the .wprp file.

1. Run `start_capture.bat` as administrator. This will start recording Windows MIDI Services activity on the system. We record related API calls, errors, etc. No screenshots are taken.
2. Recreate the issue
3. Run `save_capture.bat` as administrator to stop recording and create the `.etl` file.
4. Provide the `.etl` file to us in a secure way. These tend to be large, so OneDrive or Google Drive or other similar sharing services are preferred.

This is all very similar to what Feedback Hub does when you record a repro problem. 

Once we ship Windows MIDI Services in the box with Windows, we'll look to move this profile into Feedback Hub.

## Looking at the output yourself

If you want to look inside the `.etl` file, [install Windows Performance Analyzer](https://learn.microsoft.com/windows-hardware/test/wpt/windows-performance-analyzer) and then open the file. Double-click the System Activity on the left and you'll be able to see the events that we log in the various components. Not all events are logged, of course, but we do log key ones.