---
layout: kb
title: MIDI apps get no input after Windows tweaks (MMCSS task settings)
description: How "optimization" and latency tweaks to the Multimedia Class Scheduler Service can stop MIDI apps from receiving any input, and how to put the Windows defaults back.
audience: everyone
categories:
  - Troubleshooting
---

## What you see

A MIDI app opens your controller without any error, but nothing happens when you move a fader, turn a knob or hit a pad. Other MIDI apps on the same PC work fine with the same controller. Changing drivers, cables, USB ports or the API mode doesn't help, and the problem only happens on one PC.

We've seen this with apps built on the older `Windows.Devices.Midi` API, such as djay Pro. Apps that use WinMM (most older MIDI apps) or the Windows MIDI Services SDK usually keep working, which makes it look like a bug in the app or in Windows.

## Why it happens

Windows has a service called the **Multimedia Class Scheduler Service (MMCSS)**. Music and audio apps ask it for priority so that their audio and MIDI work isn't held up by everything else running on the PC. MMCSS hands out priority through named tasks. The two that matter for music are **Audio** and **Pro Audio**.

The settings for these tasks live in the registry, and lots of tweak guides, "debloaters" and "optimizer" apps change them. Some of those changes are harmless. Others use values Windows doesn't accept, like a **Priority** of 17 when the highest valid value is 8. When Windows starts and finds a task with a bad value, it drops that task. From then on, any app that asks for that task is turned down.

Some MIDI apps can't receive input without the **Audio** task. They still open the port, so there's no error to see. They just never get any messages.

These are Windows settings, not Windows MIDI Services settings, so reinstalling MIDI drivers or tools doesn't fix them.

## Check your PC

The easiest way is the **Registry** page of [Windows MIDI Troubleshooting and Repair]({{ site.baseurl }}/tools/miditroubleshooter/#registry). Its MMCSS section shows a red marker on any task Windows can't use, and an amber marker on a task that works but was changed. It also shows whether `MaxThreads` is set (see below).

You can also check from PowerShell. You don't need to run it as administrator. It asks Windows for each task, the same way an app does, and then gives it straight back.

```powershell
Add-Type -Namespace W -Name Avrt -MemberDefinition '[DllImport("avrt.dll", SetLastError=true, CharSet=CharSet.Unicode)] public static extern IntPtr AvSetMmThreadCharacteristicsW(string task, ref uint idx); [DllImport("avrt.dll")] public static extern bool AvRevertMmThreadCharacteristics(IntPtr h);'
foreach ($t in 'Audio','Pro Audio') { $i=[uint32]0; $h=[W.Avrt]::AvSetMmThreadCharacteristicsW($t,[ref]$i); if ($h -eq [IntPtr]::Zero) { "$t : FAILED, Win32 error " + [Runtime.InteropServices.Marshal]::GetLastWin32Error() } else { "$t : OK"; [void][W.Avrt]::AvRevertMmThreadCharacteristics($h) } }
Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile\Tasks\Audio'
Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile\Tasks\Pro Audio'
```

If either task says `FAILED, Win32 error 1550`, Windows has dropped that task.

## The Windows defaults

These are the values Windows ships with, under `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile\Tasks`.

| Value | Audio | Pro Audio |
| --- | --- | --- |
| Affinity (DWORD) | 0 | 0 |
| Background Only (string) | True | False |
| Clock Rate (DWORD) | 10000 | 10000 |
| GPU Priority (DWORD) | 8 | 8 |
| Priority (DWORD) | 6 | 1 |
| Scheduling Category (string) | Medium | High |
| SFIO Priority (string) | Normal | Normal |

Tweak guides often change two more values, one level up in `SystemProfile`. They don't cause this problem, but if you're putting things back, the defaults are `SystemResponsiveness` = 20 and `NetworkThrottlingIndex` = 10 (both DWORD).

Some guides also add a `MaxThreads` DWORD in `SystemProfile`. It changes how many threads each app can register with MMCSS. Windows doesn't create it, and without it each app can register up to 32. Unless an app vendor told you to set it, delete it.

## Put the defaults back

1. Open PowerShell as administrator.
2. Run this. It sets the values that tweak guides change most often back to the Windows defaults.

```powershell
$tasks = 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile\Tasks'
Set-ItemProperty "$tasks\Audio" -Name 'Priority' -Type DWord -Value 6
Set-ItemProperty "$tasks\Audio" -Name 'Scheduling Category' -Type String -Value 'Medium'
Set-ItemProperty "$tasks\Audio" -Name 'SFIO Priority' -Type String -Value 'Normal'
Set-ItemProperty "$tasks\Audio" -Name 'Clock Rate' -Type DWord -Value 10000
Set-ItemProperty "$tasks\Pro Audio" -Name 'Priority' -Type DWord -Value 1
Set-ItemProperty "$tasks\Pro Audio" -Name 'Scheduling Category' -Type String -Value 'High'
Set-ItemProperty "$tasks\Pro Audio" -Name 'SFIO Priority' -Type String -Value 'Normal'
Set-ItemProperty "$tasks\Pro Audio" -Name 'Clock Rate' -Type DWord -Value 10000
```

3. **Restart Windows.** This step matters. Windows reads these settings only when it starts, so the task stays dropped until you restart, even after the values are right.
4. Run the check again. Both tasks should say `OK`.

If a tweak tool you still have installed keeps putting its own values back, turn off that setting in the tool or uninstall it.

## A word on tweak guides

Lists of registry "optimizations" get copied from site to site, and many were written for much older versions of Windows. They rarely say what each change does or how to undo it. If you want lower audio latency, a better place to start is the [DPC latency check]({{ site.baseurl }}/kb/dpc-latency/), which finds the drivers that actually hold your audio up.
