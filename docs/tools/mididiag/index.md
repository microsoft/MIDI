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

The report starts with your PC and Windows, then covers each part of Windows MIDI Services. It ends with a short list of findings: things the report noticed that are worth a look, each one written as a plain sentence.

The report tells you:

- The versions of Windows, Windows MIDI Services, the MIDI service, `wdmaud2.drv` and the USB MIDI class drivers
- When Windows last started, and whether it came back from Fast Startup instead of a full restart
- Which API mode the PC uses: full, legacy or hybrid
- Whether the MIDI service is installed, how it starts, and whether it was already running before the report
- Whether each Windows MIDI Services file that comes with Windows is in System32, and whether Windows is set up to use it
- Times in the last 30 days when the MIDI service stopped unexpectedly, and app crashes that involved a MIDI component
- When Windows was installed or upgraded, recent Windows updates, the MIDI software that's installed, and when MIDI software was installed or removed
- The MIDI driver entries in the Drivers32 part of the registry, a common source of past problems
- The Windows MIDI Services registry entries, including any transport the service doesn't have permission to read, and any transport whose file is missing
- USB and MIDI devices with a problem, the ones Device Manager marks with a warning, how many old MIDI device entries Windows is still keeping, and when those were last seen
- Your network type, your network adapters, and the firewall rules for the MIDI service, which matter for network MIDI
- Whether multicast DNS (mDNS) can work on this PC. Network MIDI uses it so computers can find each other by name. The report shows the Windows service that answers it, the setting that turns it off, the firewall rules for it, and which programs listen for it.
- When networks connected and disconnected, and when the PC slept, woke and restarted, over the last 3 days
- Every Windows MIDI Services transport, endpoint and MIDI 1.0 port, with the other names each port could have had
- The device behind each endpoint: its USB vendor and product IDs, its serial number, how many hubs it's plugged in through, when it was last connected and removed, and its driver
- Every MIDI 1.0 port that WinMM apps see, and whether its number matches the one Windows MIDI Services expects
- Bluetooth MIDI, Network MIDI 2.0, RTP-MIDI and loopback details, when those transports are installed
- For each network MIDI host on this PC, whether looking it up by name still finds it
- Saved names and pictures that no longer match any device
- Every app that's connected to the MIDI service right now, and what it has open
- The results of a ping test, and how long it takes to open a MIDI connection

![mididiag]({{ site.baseurl }}/assets/images/mididiag-output-1.png) ... ![mididiag]({{ site.baseurl }}/assets/images/mididiag-output-2.png)

## Running the report

The easiest way is the [MIDI Troubleshooting and Repair]({{ site.baseurl }}/tools/miditroubleshooter/) app. Its **Diagnostics** page runs the report and saves it as a zip file. Attach the zip to your GitHub issue or email. The full report is often too long to paste into a GitHub issue, and a zip file is a fraction of the size.

From a command prompt, save the report to a file:

```
C:\Users\peteb>cd Documents
C:\Users\peteb\Documents>mididiag > mididiag.txt
```

Then send `mididiag.txt` to whoever asked for it, by email, a support form upload, or any other way you like. Zip it first if it's going into a GitHub issue.

These options change what the report includes:

| Option | What it does |
| --- | --- |
| `--include-winrt-midi1` | Also lists the ports that apps using the older WinRT MIDI 1.0 API see. Most problems don't need this, so it's off unless you ask for it. |
| `--help` | Shows the options. |

## Reading a report yourself

Select **View report** on the **Diagnostics** page of MIDI Troubleshooting and Repair. It shows the report you just ran, or opens a report somebody sent you, even inside the zip file it came in. The findings come first, then any errors in red, and every section opens and closes on its own, so you don't have to scroll through the whole text to find one device.

## When the MIDI service is stuck

The report reads everything it can from Windows before it asks the MIDI service anything. If the service doesn't answer, the report doesn't wait forever. After a short time it stops, says which section it was working on, and still lists its findings. That way you get a useful report from the PC that has the problem, which is the one that matters.

When that happens, the MIDI Troubleshooting and Repair app offers to save a memory dump of the MIDI service. The dump shows the developers where the service is stuck. Save it before you restart, because restarting the PC clears the problem and the evidence along with it. The dump can hold private information, such as device names and network MIDI passwords, so share it only with the developers who asked for it.

## When the MIDI service is missing

Windows MIDI Services comes with Windows. Its files are in the System32 folder, and Windows has to register each one before anything can use it. To an app, a missing file and a file that isn't registered look the same: MIDI just doesn't work. The `inbox_components` section tells them apart.

It has an `inbox_component` line for each file that comes with Windows:

- `file` says whether the file is in System32: `present` or `missing`.
- `service`, for `midisrv.exe`, and `com`, for the others, say where Windows loads the file from: `system32`, `elsewhere` with the path it uses instead, `missing_file` when there's no file at that path, or `not_registered`.
- `com_owner` says who owns that COM registration in the registry. Windows keeps the ones it writes owned by `trustedinstaller`, so nothing else can change them. Any other owner, usually `administrators`, means an installer took the registration over or wrote it again.
- `plugin`, for the four transports the MIDI service loads through its Transport Plugins registry entries, says whether there's an entry for that transport: `enabled`, `disabled`, `missing`, or `no_clsid` when the entry is there but empty.

The `inbox_state` line sums it all up:

| Value | What it means |
| --- | --- |
| `ok` | Every file is in System32, and Windows is set up to use it. |
| `not_registered` | The files are in System32, but none of them are registered. |
| `partly_registered` | The files are in System32, but some of them aren't registered. |
| `registered_elsewhere` | Some files are registered to load from outside System32, usually from a developer or preview copy. |
| `files_missing` | Some of the files are missing from System32. |
| `not_in_windows` | None of the files are in System32, so this copy of Windows doesn't include Windows MIDI Services. |

Uninstalling the old Windows MIDI Services preview service can leave a PC `not_registered` or `partly_registered`. That preview replaced the MIDI service that comes with Windows, so removing it left Windows without one. If the Windows Installer log still has the removal, the findings say when it happened.

To find out when MIDI last worked, look at the `endpoint_nodes` lines in the `device_nodes` section. Windows keeps an entry for every endpoint the MIDI service ever made. `last_seen` says when the newest of the ones that are gone was last there. The `DIAG` endpoints come and go with the MIDI service itself, so theirs is about when the service last ran. `first_installed` is the oldest date Windows has for them. An upgrade to a new version of Windows can reset it.

Then look at the `install_history` section to see what changed around that time:

- `os_installed` is when this version of Windows was installed, either fresh or as an upgrade.
- Each `os_upgrade` line is an earlier version of Windows, and when it was replaced.
- Each `windows_update` line is an update to Windows itself from the last 90 days, such as a monthly update. Updates to apps, drivers and Microsoft Defender are left out.
- Each `installed_package` line is a program with MIDI in its name that's installed now. Windows MIDI Services comes first.
- Each `package_event` line is a MIDI program that Windows Installer installed, removed or changed.
- `system_log_from` and `application_log_from` say how far back the Windows logs go. A history with nothing in it only means nothing happened if the log goes back far enough.

## When other computers can't see your PC

Other computers find Network MIDI 2.0 and RTP-MIDI hosts by name, using multicast DNS (mDNS). For each host that's running, the report looks it up by the same name other computers look for, and writes an `advertising_check` line with what came back. If nothing answers, the findings say other computers may not see the host. If a different computer or port answers, the findings say that too.

This PC answers for its own hosts, so the check shows whether a host is still being advertised. It can't prove that other computers hear the answer. That also depends on your network and your firewall, which the `mdns` section covers. A host that doesn't answer adds about 2.5 seconds to the report.

The `network_history` section lists when networks connected and disconnected, and when the PC slept, woke and restarted, over the last 3 days. Lining those times up with when the problem started often explains it.

## Privacy

The report leaves out the things that identify you:

- Folders inside your user profile are written as `%USERPROFILE%`, and other people's profile folders as `<user>`, so your user name isn't in the report.
- Only the last part of an IP address is kept, for example `x.x.x.27` or `x::7d8d`. That includes the addresses of your network adapters and the addresses in firewall rules.
- Network names, such as the name of your Wi-Fi network, are left out. The network history gives each network a number instead.
- The hardware (MAC) addresses of your network adapters are left out.

Device serial numbers and Bluetooth addresses stay in, because they're how the developers tell two of the same device apart. The names you've given your devices stay in too. So do the names and versions of installed programs that have MIDI in their names, because they often explain a problem. If any of that matters to you, read the report before you post it somewhere public.

## Reading the report with a script

The report is plain text with a few simple rules, so a script can read it without guessing:

- The lines before the first section are a short heading for people. Skip them.
- A section starts with a line that begins with `== ` and then the section name, for example `== enum_sessions`.
- Every other line that isn't blank is a field: a label, then spaces to line it up, then ` : `, then the value. Labels never contain spaces, so the first ` : ` on a line is the one that ends the label.
- A blank line separates the records in a section, such as one endpoint from the next.
- Some values hold several parts as `key=value` pairs separated by spaces, for example `flow=out number=3 group=1 name="Port 1"`. A value that's empty or holds a space, an equals sign or a double quote is put in double quotes, and a double quote inside it is written twice, the same way a CSV file does it.
- Section names, labels and keys are never translated. Finding texts and error messages can be.
- The `header` section has a `report_format_version` field. It goes up only when a change could break a script that follows these rules, such as a label that's renamed or removed. New sections, labels and keys don't change it, so have your script skip anything it doesn't recognize. Don't rely on the order of sections, fields or keys either.
- Near the end, the `findings` section has one `finding` line for each thing the report noticed. Its `id` is the same in every language, so match on that rather than on the text.
- The `section_timing` section says how long each section took.
- The report ends with a `successful_run` or `aborted_run` section, and then `end_of_file`. A report without `end_of_file` was cut short.

All the labels are defined in `mididiag_field_defs.h` in the mididiag source folder.

If your tool is written in C++, you don't have to write the parser yourself. `mididiag_report_parser.h` and `mididiag_report_parser.cpp`, in the same folder, read a report into sections, records and fields, split the `key=value` pairs and list the findings. They also read reports from older versions of mididiag, and report files that PowerShell saved as UTF-16. Copy those two files and `mididiag_field_defs.h` into your project. They need C++20 and the standard library, and nothing else. The report viewer in MIDI Troubleshooting and Repair uses the same files.

All times in the report use the PC's own time zone, written as YYYY-MM-DD HH:MM:SS with a 24-hour clock.

## Exit codes

| Code | Meaning |
| --- | --- |
| 0 | The report finished. |
| 3 | An option wasn't recognized. |
| 4 | The console couldn't be set up. |
| 5 | The MIDI service stopped answering, so the report stopped early. |
| 6 | A section took too long, so the report stopped early. |
| 99 | Something else went wrong. The report says what. |
