# Configurations window for MIDI Settings: plan and handoff

> **UNSUPPORTED — DO NOT COPY THIS APPROACH.** This code reads and writes the Windows MIDI Services configuration file directly. Microsoft does not support direct manipulation of that file by anything other than the in-box MIDI tools. Its format, its name, its location and the registry value that selects it are implementation details and can change in any release without notice, so this code can corrupt a configuration or stop working at any time. It exists for development and diagnostics inside the Windows MIDI Services repository only. Applications and third-party tools must use the Windows MIDI Services API (`Windows.Devices.Midi2.ServiceConfig`) instead.

This plan is for whoever builds the window. It covers what was decided, what's in a package, how import merges, how the code is split up, and what already exists. The package format below is internal to the in-box MIDI tools. It isn't a supported format, and it can change at any time.

## Where things stand (October 14, 2026)

- The comps are in this folder. Open [index.html](index.html) for the list of pages. Each page takes `?state=` variants, listed in its caption.
- [check.ps1](check.ps1) checks the comps for overlaps and cut-off text. [capture.ps1](capture.ps1) takes the screenshots. Both use headless Microsoft Edge.
- No code for the window exists yet. Pete asked for comps first.
- MIDI Settings already has a smaller version in the Global MIDI settings card: Back up now, Add a configuration file, and Restore from a backup. It's in `MidiConfigBackups.cpp` and `MainWindowConfigBackups.cpp`. It stays until this window ships. Then the card gets smaller (decision 6).

## Decisions (Pete, October 14, 2026)

1. **Import writes the configuration file, then the MIDI service restarts to use it.** It doesn't go through the running service.
2. **When a package disagrees with this PC, the customer's version wins by default.** That covers device names, the synthesizer mode and port names. A loopback or network host whose name is already taken is left out unless the customer ticks its box. A network host named after the other PC takes this PC's name, ticked by default.
3. **The package file type is `.midiconfigpack`.** The Tools installer registers it, so double-clicking a package opens Import.
4. **Packages carry the port names setting for older apps.** It goes in its own entry inside the package, not inside the configuration file, and never as a `.reg` file.
5. **Backups made by MIDI Settings are full packages:** the configuration, its pictures, and the settings that live outside the file. Pete left room for a "just the configuration file" choice. The proposal is full backups only, because the daily backups Windows MIDI Services makes already hold just the configuration.
6. **Global MIDI settings drops Create, Back up now, Add and Restore** and keeps only the name of the configuration in use and a Manage configurations button. Do this in the same change that adds the window, not before.

## What the customer gets

- A Configurations button pinned on the right half of the main window's toolbar opens a separate window (comp 1).
- **Configurations** lists the configuration files and shows which one is in use. Create, rename, duplicate, delete, and switch, with a prompt to restart the MIDI service (comp 2).
- **Backups** lists every backup, newest first, with why it was made. Picking one shows what's in it next to what's in use now. Restore, Share and Delete, plus Delete old backups (comp 3).
- **Share** saves a package of a configuration with its pictures and settings, with an optional note (comp 4).
- **Import** opens a package, or a configuration file saved before packages existed (comp 5). Review shows every part with a choice and a plain line about what will happen (comp 6). Done lists the results, offers the restart, and offers Undo this import (comp 7).
- Most actions need an administrator. Without one, the window shows what's there and offers Reopen as administrator.

## The package file

A `.midiconfigpack` is a zip file written with `ZipWriter` and read with `ZipReader` from `src/in-box/user-tools/midi-app-shared/ZipArchive.h`, the same code MIDI Glass and MIDI Sequencer use. `ZipWriter` writes beside the final name and renames it when it's finished, so a failed save never leaves half a file.

| Entry | What's in it |
| --- | --- |
| `package.json` | Format version, kind (`share` or `backup`), configuration name, note, computer name, date and time (UTC), the reason (backups only), the app version, and which parts are inside with their counts. |
| `configuration.json` | The configuration file's bytes, exactly as they were. |
| `settings.json` | Settings that live outside the configuration file. For now only the port names setting, stored by name: `classicCompatible`, `newStyle` or `automatic`. Never stored as a number or as registry data. |
| `pictures/<file name>` | Every picture the configuration names, under the same bare file name the configuration uses. |

Reading a package:

- **Treat every package as untrusted.** It may come from another PC or from the internet.
- Limit the number of entries, the size of each entry, and the total size. The configuration keeps today's 16 MB limit.
- Accept only the names in the table above. A picture name must be a bare file name with one of the picture types the picture chooser offers. Ignore anything else, and never use a name from the zip as a path.
- Pictures go into the shared pictures folder only through `EndpointImageAssets::CopyIntoFolder` (in `midi-app-shared`). It cleans names, reuses an identical file, and gives a different file with the same name a "(1)" suffix. It takes a source path, so write each picture to a temporary file with a generated name first. It returns the stored name, which may differ from the one in the package, so the merged configuration has to use the returned name.
- Refuse a package whose format version is newer than the app understands, and say so.
- Use the computer name in `package.json` only for display and for spotting hosts named after the other PC.
- A plain `.midiconfig.json` from an older version is accepted too. It has no pictures and no settings, and the review page says so.

## Backups

- Folder: `%ALLUSERSPROFILE%\Microsoft\MIDI\Backups\`.
- Name: `<configuration name> <yyyy-MM-dd HHmmss>.midiconfigpack`, with "-2", "-3" and so on added if the name is taken. Create the file only if it doesn't already exist. The reason lives in `package.json`, so the file name stays short.
- A backup is a package with kind `backup`: the configuration, its pictures, and `settings.json`.
- MIDI Settings makes one before an import, before a restore, before deleting a configuration, and whenever the customer asks.
- The daily backups Windows MIDI Services makes (`<configuration file>.<yyyy-MM-dd>.bak`, beside the configuration) show in the list as "Configuration only" and restore just the configuration. The `.bak` files today's Back up now button makes (`<configuration file>.<yyyy-MM-dd-HHmmss>.bak`) are listed the same way.
- Restoring a full backup puts back the configuration, copies its pictures in, and sets the port names setting. Then it offers the restart.
- Nothing deletes backups by itself. Delete old backups keeps a time window and the newest few of each configuration (comp 3, `?state=cleanup`).
- Deleting needs an administrator. The folder lets any signed-in user write but not delete.

## How import works

1. Read the package or plain file, and list its parts and items.
2. Read the configuration in use, because that's what gets merged into. Read the port names setting with `DefaultMidi1PortNaming()` in `MidiConfig.cpp`. Ask the API which transports are installed (`GetInstalledTransportPlugins`), so RTP-MIDI can say it starts working once it's installed.
3. Work out every item's status, problem and default choice. Defaults keep the customer's version (decision 2).
4. The customer reviews and chooses (comp 6).
5. Apply:
   1. Make a full backup with the reason "Before importing from <computer name>".
   2. Copy the pictures in and note each stored name.
   3. Merge into an in-memory copy of the configuration, using the rules below and the stored picture names.
   4. Check the result: it parses, and it still has the `endpointTransportPluginSettings` object.
   5. Write it in place, the way `PutInPlace` in `MidiConfigBackups.cpp` already does. Hold the file open for reading and writing, sharing read only. That handle is the same write lock the SDK uses when it saves a transport section, so the two never write over each other. Refuse links and reparse points. Retry sharing violations for a short while. If the file changed since step 2, stop and plan again instead of writing over someone else's change.
   6. If the customer chose the package's port names, set them with `SetDefaultMidi1PortNaming()`.
   7. Return one result per item (comp 7).
6. Done offers Restart MIDI service (`RestartService()` in `MidiConfig.cpp`). Nothing restarts without the customer choosing it.
7. Undo this import restores the backup from step 5.1.

"Keep it as a new configuration" writes a new `<name>.midiconfig.json` instead of merging. It still fixes clashing names and other-PC hosts. It isn't made the one in use unless the customer switches to it.

The MIDI service and the transports only read the configuration file. Apps change it through the SDK, which merges one transport section at a time under the same lock. So another app saving a setting between the import and the restart keeps the import's changes.

## Merge rules

Everything the service reads is under `endpointTransportPluginSettings`, one section per transport, keyed by the transport's ID:

| Transport | ID |
| --- | --- |
| USB and other KS devices | `{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}` |
| KS aggregate devices | `{0F273B18-E372-4D95-87AC-C31C3D22E937}` |
| MIDI 2.0 loopbacks | `{942BF02D-93C0-4EA8-B03E-D51156CA75E1}` |
| Basic MIDI 1.0 loopbacks | `{10088473-9478-4E62-850B-3D2315E135B8}` |
| Network MIDI 2.0 | `{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}` |
| RTP-MIDI | `{54C9B2F6-C235-4000-A675-9F6958A1A4FA}` |
| Bluetooth MIDI | `{5DC87270-F318-4838-A4F9-6AADC63E925F}` |
| General MIDI synthesizer | `{7605713E-FEA9-409D-A90F-A81233200D0A}` |

Compare section keys without regard to case. Keep this PC's `header`.

- **Device names and pictures.** These are the entries in a section's `update` array. An entry's `match` object says which device it's for, so the same `match` means the same device.
  - Add to mine (default) adds entries whose `match` isn't here. For a device that's in both, Keep mine (default) or Use the package's.
  - Replace mine removes this PC's entries first. The review lists exactly what goes.
  - Don't import skips them. Pictures come along only with the entries that are imported.
  - Some match keys only fit the PC they were made on, like `endpointId` and `deviceInstanceId`. Those entries still come across. The existing "Some saved settings have no matching device" banner and its Review them button already handle entries that never match (Pete: "our existing orphaned customizations detection should handle problems").
- **Loopbacks** (both kinds) are under `create`. The names the customer sees are the endpoint names.
  - A loopback whose name matches one here is left out unless its box is ticked. Ticked, it gets a free name, such as "Max 2".
  - If its `associationIdentifier` is already used here, give it a new one.
  - Check the exact shapes in `MidiLoopbackCreationConfig.cpp` and `MidiBasicLoopbackCreationConfig.cpp` (in `src/in-box/Client/WinRT/core`) before writing the merge.
- **Network MIDI 2.0.** Hosts are under `create.hosts` and clients under `create.clients`, each keyed by an entry ID. Changes made after creation are in `updateEntries`. Keys are in `src/in-box/Transport/UdpNetworkMidi2Transport/network_json_defs.h`.
  - A host named after the other PC takes this PC's name, ticked by default. Change `name`, `serviceInstanceName`, and the host's product instance ID. `MidiNetworkHostCreationConfig.cpp` (in `src/in-box/Client/WinRT/core`) shows how a new host builds all three from the PC, and where the ID is stored.
  - A host whose name or service name is taken here works like a taken loopback name.
  - The adapter keys (`networkAdapterId`, `networkAdapterPhysicalAddress`, `networkAdapterName`) name an adapter on the other PC. Set `allowNetworkAdapterFallback` to true, and say "uses any adapter here".
  - A client is added unless one with the same `match` is already here.
  - If an entry ID is already used here, give the entry a new ID, and carry its `updateEntries` over under the new ID.
  - Keep this PC's `transportSettings`.
- **RTP-MIDI** uses `create.hosts` and `create.clients` with the same rules. Keys are in `src/in-box/Transport/RtpMidiTransport/rtp_json_defs.h`. It's saved even when RTP-MIDI isn't installed, because it's only text in the file.
- **Bluetooth.** Merge the `devices` array by `deviceId`. For a device that's in both, keep this PC's entry. Keep this PC's `peripheral` object, because it describes this PC and the Bluetooth hosts it has allowed or denied.
- **General MIDI synthesizer.** Keep mine (default) or Use the package's synthesizer mode (`synthMode`). The audio output settings stay with the PC, because they depend on its sound hardware.
- **Port names for older apps.** Keep mine (default) or Use the package's. If the package says `automatic` and this PC can't use it, keep this PC's setting and say so.
- **A section for a transport not listed here** is copied only when this PC has no section for it, and the review lists it.

## How the code is split up

Pete: "keep this code self-contained. The MIDI Settings app is the app most likely required to redesign the UI once it goes into Windows, so I want to make sure that we can pull in and reuse all the code." No new API.

- **An engine with no user interface** in its own folder, for example `src/in-box/user-tools/midi-settings/config-engine/`. Plain C++ and `Windows.Data.Json`: no XAML, no app strings, no precompiled header from the app. It returns data and reason codes, and the window turns those into words from `Resources.resw`.
  - `ConfigWrite`: the locked, in-place write and the checks from today's `MidiConfigBackups.cpp`.
  - `ConfigFiles`: list, create, rename, duplicate, delete, and switch the configuration in use. Today's file functions in `MidiConfig.cpp` move here or get wrapped.
  - `ConfigPackage`: write and read `.midiconfigpack`.
  - `ConfigInventory`: turn a configuration or package into parts, items and counts.
  - `ConfigBackups`: make, list, restore and delete backups, including the "Configuration only" ones.
  - `ImportPlan`: compare a package with this PC and work out statuses, problems and default choices.
  - `ImportApply`: back up, copy pictures, merge, write, set port names, and return results.
- Every engine file that touches the configuration file or a package carries the DO NOT COPY banner at the top of this document.
- SAL on every parameter, `_Use_decl_annotations_` in the `.cpp`, and no exception escapes an engine function.
- **The window** is a new `ConfigurationsWindow` in its own files, with one file per page. `MainWindow` gets only the toolbar button and its click handler. The toolbar's pinned-width numbers in `ApplyToolbarLayout` have to grow to fit the button. Only one Configurations window is open at a time; clicking again brings it forward.
- **Elevation**: use `TryRelaunchElevated()` in `Elevation.cpp` and `App::IsElevated()`. Add launch switches to `settings_app_launch_defs.h`, one that opens this window and one that opens Import with a file, so Reopen as administrator lands back in the same place.
- **File type**: add `ConfigPackFileAssociation.wxs` beside `PatchbayFileAssociation.wxs` and `GlassFileAssociation.wxs` in `src/installers/api-and-tools-installer/sdk-package/`, and follow the same rules. A copy that's already running gets the file the way MIDI Patchbay does, with `WM_COPYDATA` (see `LibraryWindow.xaml.cpp`).
- **Strings** go in `Resources.resw`, added a few at a time. Every control gets an accessible name, and results are read out through a live region. Run `build/check_accessibility.ps1` and `build/check_en_us_spelling.ps1`.

## Order of work

1. `ConfigWrite`, `ConfigPackage` and `ConfigBackups`, with tests.
2. `ConfigInventory`, `ImportPlan` and `ImportApply`, with tests.
3. The window, the toolbar button, and the Configurations and Backups pages.
4. The Share and Import pages, the results, and the restart.
5. The file type, the launch switches, and the running-copy handoff.
6. Slim the Global MIDI settings card. Remove the controls, handlers and strings it no longer needs, and move what's left of `MidiConfigBackups.cpp` into the engine.
7. Docs: `docs/tools/settings/index.md`, the backup and move answers in `docs/kb/common-midi-tasks.md`, and the Network MIDI 2.0 article's line that copying the configuration to another PC carries the setup with it. That line tells customers to copy the file by hand, which isn't supported. The copy also replaces the other PC's whole configuration, and brings the first PC's host identity and adapter with it. Point it at Import instead. Retake the screenshots.

## Tests

- A TAEF test project for the engine, following `src/in-box/Test/Tools/Midi2.MidiPatchbay.unittests/`.
- Package: round trip; too many entries, entries too big, bad checksums, names with `..`, a drive or a leading slash, unknown names, a newer format version, and a plain `.midiconfig.json`.
- Merge: every rule above. That includes clashes and free names, new IDs with their `updateEntries`, other-PC hosts, adapter fallback, keep-mine defaults, Replace mine, and an unknown transport section.
- Write: a file changed between plan and apply, a link or reparse point, a sharing violation that clears, and identical bytes.
- Backups: listing full backups next to both kinds of `.bak` file, restoring each kind, and the cleanup rule.
- Port names: each stored name, and `automatic` on a PC that can't use it.

## Still open

- Whether to offer "just the configuration file" backups too (proposal: no).
- The numbers for Delete old backups. The comp shows 30 days and the newest 5 of each configuration.
- Whether "Keep it as a new configuration" is worth keeping in Import.
