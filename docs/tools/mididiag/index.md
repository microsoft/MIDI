---
layout: tools_page
title: MIDI Diagnostics Report
tool: mididiag
description: All about the mididiag tool - a reporting tool for troubleshooting and for technical support.
---

When seeking technical support, the support team will often want to check basic operation of your system. At the request of MIDI hardware partners, the `mididiag.exe` tool has been provided as a way to supply a machine-readible text dump of the state of MIDI on your PC.

The report can be quite long, but contains some basics about your system and then the status of various parts of Windows MIDI Services.

From this report, the viewer can identify
- The version of Windows MIDI Services you have installed
- The version of your operating system
- The processor type for your operating system (Arm64, x64, etc.)
- The time resolution on your PC
- Whether or not you have developer mode enabled
- The state of the midi drivers entries in the drivers32 location in the registry (a common source of past issues)
- All the MIDI 1 API endpoints enumerated using WinRT MIDI 1.0
- All the MIDI 1 API endpoints enumerated using WinMM MIDI 1.0
- The Windows MIDI Services-specific registry entries and their values
- The status of the SDK installation
- The list of all installed and enabled Windows MIDI Services transports
- The list of all Windows MIDI Services endpoints, and their mapped MIDI 1.0 ports, their group terminal blocks, their name tables, and more
- The results of a ping test

![mididiag]({{ site.baseurl }}/assets/images/mididiag-output-1.png)
...
![mididiag]({{ site.baseurl }}/assets/images/mididiag-output-2.png)


Typically, when asked to provide the output, you will type something like this:

```
C:\Users\peteb>cd Documents
C:\Users\peteb\Documents>mididiag > mididiag-output.txt
```
<br/>
That causes the output from `mididiag.exe` to be stored in the file named after the redirection symbol.

Then you will supply the `mididiag-output.txt` to them via email, a support form upload, or other transfer mechanism.

## Notes

We may add more fields or sections in the future. If you are machine parsing this file, do not rely on the order of fields or sections. The actual names of fields shown and the section headers will remain static and can be used in machine parsing, however. The tokens are not localized.

The date at the top of the file is in YYYY-MM-DD format. Time is in 24-hour format.

The executable returns 0 when succeeded, and non-zero when failed.
