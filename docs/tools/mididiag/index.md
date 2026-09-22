---
layout: tools_page
title: MIDI Diagnostics Report
tool: mididiag
description: All about the mididiag tool - a reporting tool for troubleshooting and for technical support.
icon: /assets/images/mididiag-output-1.png
categories:
  - Diagnostic Tools
---

When you ask for technical support, the support team usually wants to check that the basics are working on your PC. At the request of MIDI hardware partners, `mididiag.exe` produces a text report of the state of MIDI on your PC that both a person and a script can read.

The report can be long. It starts with some basics about your PC and then covers the state of each part of Windows MIDI Services.

The report tells you:

- The version of Windows MIDI Services you have installed
- The version of your operating system
- The processor type for your operating system (Arm64, x64 and so on)
- The time resolution on your PC
- Whether developer mode is enabled
- The state of the MIDI driver entries in the Drivers32 part of the registry, a common source of past problems
- All the MIDI 1.0 endpoints seen through WinRT MIDI 1.0
- All the MIDI 1.0 endpoints seen through WinMM MIDI 1.0
- The Windows MIDI Services registry entries and their values
- The status of the SDK installation
- Every installed and enabled Windows MIDI Services transport
- Every Windows MIDI Services endpoint, with its MIDI 1.0 ports, its group terminal blocks, its name tables and more
- The results of a ping test

![mididiag]({{ site.baseurl }}/assets/images/mididiag-output-1.png) ... ![mididiag]({{ site.baseurl }}/assets/images/mididiag-output-2.png)

To save the report to a file, redirect the output:

```
C:\Users\peteb>cd Documents
C:\Users\peteb\Documents>mididiag > mididiag-output.txt
```

Then send `mididiag-output.txt` to whoever asked for it, by email, a support form upload, or any other way you like.

The [MIDI Troubleshooting and Repair]({{ site.baseurl }}/tools/miditroubleshooter/) app can run this same report for you, with buttons to copy or save the output, if you'd rather not use a command prompt.

## Notes

We may add fields or sections in the future. If you're parsing this file with a script, don't rely on the order of the fields or sections. Field names and section headers stay the same, so it's safe to match on those. They aren't translated.

The date at the top of the file is in YYYY-MM-DD format. Time is in 24-hour format.

The program returns 0 when it succeeds, and non-zero when it fails.
