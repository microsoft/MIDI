---
layout: tools_page
title: MIDI1Monitor tool Overview
tool: midi1monitor
description: All about the midi1monitor tool
---

`midi1monitor.exe` is a very simple MIDI input / source monitor supplied as part of the SDK. It uses the WinMM MIDI 1.0 API to display messages on an incoming port. 

This app is designed primarily for simple use-cases.

![midi1monitor](/assets/images/midi1monitor.png)

The raw MIDI 1.0 byte data is shown on the left in hexidecimal. The interpretation of that data is shown to the right of the raw data. Numbers in the interpretation are in decimal format.

By default, active sense and clock messages are hidden, as they tend to make monitoring other messages more difficult. Hit the spacebar to toggle displaying them.

To exit the app, hit escape.
