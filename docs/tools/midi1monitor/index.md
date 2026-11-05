---
layout: tools_page
title: MIDI1Monitor tool Overview
tool: midi1monitor
description: All about the midi1monitor tool
icon: /assets/images/midi1monitor.png
categories:
  - Developer and Technical User Tools
---

`midi1monitor.exe` is a very simple monitor for a MIDI input port, supplied as part of the SDK. It uses the WinMM MIDI 1.0 API to show the messages arriving on a port.

It's built for simple jobs. For anything more, use [MIDI Monitor]({{ site.baseurl }}/tools/midi2monitor/). To watch a port through the original `Windows.Devices.Midi` WinRT API instead of WinMM, use [midi1rtmonitor]({{ site.baseurl }}/tools/midi1rtmonitor/).

![midi1monitor]({{ site.baseurl }}/assets/images/midi1monitor.png)

Each message gets its own line. On the far left is the WinMM timestamp: the time in milliseconds since the tool started the port. It's the same timestamp WinMM hands every app with each message, so it shows what an app would see. Next are the raw MIDI 1.0 bytes, in hexadecimal. What those bytes mean is on the right, with numbers in decimal.

Active sensing and clock messages are hidden by default, because they make everything else hard to read. Press the spacebar to show or hide them.

Press escape to exit.

## App provenance

This app was originally coded by Pete Brown. It has been modified and fixed using GitHub Copilot since that time.
