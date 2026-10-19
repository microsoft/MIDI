---
layout: tools_page
title: MIDI1RTMonitor tool Overview
tool: midi1rtmonitor
description: A MIDI 1.0 input monitor that uses the original Windows.Devices.Midi WinRT API, for debugging
categories:
  - Developer and Technical User Tools
---

`midi1rtmonitor.exe` is a simple monitor for a MIDI 1.0 input port. It uses `Windows.Devices.Midi`, the WinRT MIDI API that came with Windows 10. Otherwise it works just like [midi1monitor]({{ site.baseurl }}/tools/midi1monitor/), which uses WinMM.

It's a debugging tool. Apps built on `Windows.Devices.Midi` reach MIDI a different way than WinMM apps do. So when one of those apps misbehaves, watch the same port with this tool and with midi1monitor, and compare what each one shows. For everyday monitoring, use [MIDI Monitor]({{ site.baseurl }}/tools/midi2monitor/).

Run it with a port number, like `midi1rtmonitor 3`. Run it with no number to see the list of input ports and pick one. The numbers come from `Windows.Devices.Midi`, so they don't match the WinMM port numbers midi1monitor uses. Under the port name, the tool shows the port's device ID, so you can tell apart two ports with the same name.

Each message gets its own line:

- On the far left is the timestamp `Windows.Devices.Midi` gave the message. It's the time in milliseconds since the port was opened, shown to a ten-thousandth of a millisecond, which is as fine as the API reports it.
- Next are the raw MIDI 1.0 bytes, in hexadecimal, exactly as the API handed them over.
- What those bytes mean is on the right, with numbers in decimal.

Active sensing and clock messages are hidden by default, because they make everything else hard to read. Press the spacebar to show or hide them.

Press escape to exit. The tool then shows how many bytes arrived and how long they took.
