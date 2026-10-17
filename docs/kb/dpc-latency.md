---
layout: kb
title: Find what makes audio late on your PC (DPC latency)
description: Why some drivers make audio click, pop and drop out, and how the DPC latency check in Windows MIDI Troubleshooting and Repair finds them
audience: everyone
categories:
  - Troubleshooting
---

> This page covers information about a Windows MIDI Services feature and application that will be released to consumers in 2027. It's currently available for developers.

You hear a click, a pop or a short gap in your audio, and your audio app's meters say it wasn't busy. Sometimes the cause isn't the audio app at all. It's a driver somewhere else on the PC, such as the Wi-Fi, graphics or storage driver, that kept a processor busy for too long at exactly the wrong moment.

This article explains what's going on, in plain words, and shows you how the **DPC latency check** in [Windows MIDI Troubleshooting and Repair]({{ site.baseurl }}/tools/miditroubleshooter/#dpc-latency-check) finds the driver responsible and what to do about it.

## It isn't a benchmark

The DPC latency check isn't a benchmark, and its numbers aren't a score for your PC. Use it when you already hear clicks, pops or dropouts at a buffer size that should be big enough for your project. A big project, with lots of tracks and plug-ins, needs a bigger buffer than a small one, and that's normal.

Long DPCs are common, and most of them never touch your audio. Windows spreads its work across all of the PC's processors, and a long DPC often runs on a processor your audio isn't using at that moment. That's why the check only counts something against your PC when it actually held up audio or MIDI, and never just because a number was high.

If your audio sounds fine, there's nothing to fix. Don't chase the numbers. Go make music.

## What DPC latency is

When a device needs attention, such as a network adapter with new data, it interrupts a processor. Windows answers the interrupt right away, and the driver usually asks for a bit more work to run as soon as possible. That bit of work is a DPC, short for deferred procedure call.

Interrupts and DPCs run ahead of every app. While one runs, nothing else can use that processor, not even the audio app that's waiting to make the next bit of sound. Most of them finish in a few microseconds (millionths of a second), and you'd never notice. One that takes a millisecond or more can cause a glitch, but only if audio needs that same processor at that moment.

DPC latency is the general name for the delay this causes. It's not a Windows MIDI Services setting, and nothing you set in your audio app changes it.

## Why it makes audio drop out

Audio apps make sound in small chunks called buffers. The buffer size setting in your audio app decides how big each chunk is. At 128 samples and 48 kHz, each buffer holds 2.7 ms (milliseconds) of sound, and the app has to fill the next one before the current one finishes playing.

If a driver holds up the audio app's thread for longer than that, the buffer isn't ready in time. You hear a click, a pop or a short gap. That's why a bigger buffer size often hides the problem: it gives the app more time. The cost is that everything you play sounds a little later, which is what you're trying to avoid when you play live.

MIDI can be held up the same way. A held-up MIDI message arrives late, so notes can sound uneven.

## Run the check

Run it when you hear glitches, not to see how your PC scores.

1. Open **Windows MIDI Troubleshooting and Repair** and go to **DPC latency check**. Recording needs administrator rights, so let the app restart as administrator if it asks.
2. Choose how long to check. Two minutes is a good start. Choose **Until I stop it** if your problem happens now and then.
3. Leave **Use a test thread** on. It works like an audio app's audio thread, so the check can tell how late audio would be even with no audio app open.
4. Select **Start check**, then use your PC the way you do when you hear problems. The best results come from playing your project in your audio app, with your audio interface, MIDI devices, Wi-Fi and everything else set up the way you normally have it.
5. Wait for the check to finish, or select **Stop**.

The check doesn't change anything on your PC. It doesn't install a driver, change your power settings or change how often Windows' timer ticks, and it stops recording when it's done, or when you close the app. It runs on both x64 and Arm64 PCs.

## Read the results

### The answer

The colored bar at the top gives you the answer in one line. It comes only from what the check saw being held up: your audio app, the MIDI service, or the test thread standing in for audio. A long DPC on its own doesn't change it.

- **Nothing was held up long enough to matter.** Nothing waited half a millisecond or more. If you still hear glitches, they're probably coming from something else, such as a project that needs a bigger buffer size.
- **Only short hold-ups.** Something waited between half a millisecond and a millisecond. That only matters at the very smallest buffer sizes.
- **Something held up audio or MIDI.** Something waited a millisecond or more. That's long enough for dropouts at the small buffer sizes people use to play live.
- **The check didn't get what it needs.** Nothing was running that it could watch, or the trace is missing something. It says what to do.

Under it, the check tells you how often audio, or the test thread standing in for it, was held up for a millisecond or more, and how long the longest wait was. When audio was held up, it also tells you which buffer sizes can drop out when that happens, so you know what to choose in your audio app until you fix the cause.

### What held up audio or MIDI

Each card is a driver that actually held up audio, MIDI or the test thread, worst first. It says:

- which driver it is, and when the check can tell, the company that made it, its version and the device it drives
- how long it kept a processor busy, and how often audio was waiting for it
- what to try

Some drivers in Windows, such as the driver framework, the network stack and the storage stack, run other drivers' code. The check looks inside them and names the driver that actually used the time, such as your Wi-Fi or USB controller driver, rather than the Windows part it ran inside.

Not every hold-up comes from a DPC. Some code holds a processor so tightly that nothing else runs there, not even its timers. The check catches that through the test thread: when the test thread's timer goes off late and no DPC or interrupt explains it, the check names the code that processor was running instead. Sometimes that's Windows itself.

### Also ran long

This short list shows drivers that kept a processor busy for half a millisecond or more, but didn't hold up audio, MIDI or the test thread for a millisecond or more. That's common, and usually harmless: they ran on a processor that audio wasn't using at that moment. They're only worth a look if you still hear glitches after you've tried everything else.

### What to try

The advice depends on the kind of driver:

| Kind of driver | What to try |
| --- | --- |
| Wi-Fi | Get the newest driver from the company that made it. Turn off Wi-Fi, or use a network cable, while you play or record. |
| Network adapter or Windows networking | Pause big downloads, cloud sync and streaming while you play or record. |
| Bluetooth | Turn off Bluetooth while you play or record, if you don't need it. |
| Graphics | Get the newest graphics driver. Close games, videos and other apps that draw a lot while you make music. |
| Storage | Avoid copying big files, backups and virus scans while you record. |
| USB controller | Plug your audio interface straight into the PC instead of into a hub, or try another USB port. The newest chipset drivers from your PC's maker can help too. |
| Virtual machines | Close virtual machines, Windows Subsystem for Linux (WSL), Docker and Windows Sandbox while you make music. |
| Power management | Choose **Best performance** for the power mode in Settings, and plug in a laptop. |
| Security software | Don't turn it off. Make sure it's up to date. |

Never turn off security features, such as Microsoft Defender, Memory Integrity or virtualization-based security, to make audio work better. The check never suggests it, and neither do we.

Once in a while, a processor stops answering and nothing Windows records shows why. When the test thread runs into that, the check tells you. It can come from the PC's firmware, or from virtual machines. Look for a firmware (BIOS or UEFI) update from your PC's maker, and close virtual machines while you make music.

### Notes

The notes at the bottom say what limited the check. For example, when no audio app was playing, the answer comes from the test thread alone, and it's worth checking again while your audio app plays.

## The technical report

**View technical report** opens the full results in the app's report viewer. It's the same kind of report as mididiag's, so **Save report** and **Copy report** give you a text file you can attach to a support request or send to the company that made a driver. It includes:

- at the top, the findings: only the things that need a look, in plain sentences. When nothing does, the list is empty.
- every driver that ran a DPC or an interrupt, or held a processor outside of one, how many times, the longest and the total
- for each driver: the file, company, version, devices, the processors it ran on, its routines, and which code was running inside them
- each time audio was held up, which thread, on which processor, and what ran instead
- the audio threads it found, the processors, the test thread's timing and details about the recording

The report includes the names of the programs and drivers that were running on your PC. Read it before you post it anywhere public.

## For experts

**Save a trace file for Windows Performance Analyzer** saves everything the check recorded as an .etl file. Choose where to save it before the check starts. It can be a gigabyte or more for a long check. Open it in Windows Performance Analyzer to see every DPC, interrupt and context switch, or attach it to a report for a driver's maker.

**Open a trace file** reads a trace saved earlier, by this check or by Windows Performance Recorder, without administrator rights. A trace from Windows Performance Recorder needs CPU usage with context switches, DPCs and interrupts, such as the one `wpr -start CPU` records. Driver companies and versions are shown only when the trace came from this PC and its drivers haven't changed since.

From a command prompt, `miditroubleshooter --latency-trace <file.etl>` opens the page and reads that trace straight away, without asking for administrator rights.

How the check works:

- It records kernel events with Event Tracing for Windows (ETW), in a session of its own: DPCs, interrupts, context switches, threads made ready to run, image loads, hard page faults and a profile sample about once a millisecond.
- Windows' driver framework events name the driver behind each framework DPC and interrupt. For other shared Windows drivers, the profile samples taken while the DPC or interrupt ran show whose code was running.
- An audio thread is one that ran at real-time priority, 16 or higher, nearly every time it ran. Audio apps get there through the Multimedia Class Scheduler Service (MMCSS). Threads that Windows lifts there only for a moment, and Windows' own display and input threads, don't count. The MIDI service's real-time threads are reported on their own.
- The test thread uses MMCSS "Pro Audio" priority and a high-resolution timer that wakes it every millisecond. When a wake is late, the check looks at what ran on the processor that runs the timer. It counts DPC and interrupt time there. It also counts a thread that held that processor so even its timers had to wait: one thread ran there the whole time without a switch, and at least two profile samples caught it in kernel code and none in app code. Whatever else made the timer late is the timer's own slack, which every timer has, so it doesn't count.
- When that processor ran one thread for 2 ms or more, and twice the time between its samples, without taking a single profile sample, nothing in the trace can show what held it. The check gives the firmware and virtual machine advice instead of naming a driver, and doesn't change the verdict.
- A wait or interruption of a millisecond or more is a late moment. Each one is put down to whatever ran on that processor instead: a DPC, an interrupt, another thread or a processor waking up from a power-saving state.
- The answer at the top comes only from how long audio threads, the MIDI service's threads and the test thread actually waited. A driver is listed under **What held up audio or MIDI** only when it was the biggest cause of a late moment. A driver whose DPCs or interrupts ran for half a millisecond or more without causing one goes under **Also ran long** instead, and doesn't change the answer.
