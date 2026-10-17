---
layout: sdk_reference_page
title: MidiApiContracts
namespace: Windows.Devices.Midi2
type: apicontract
idl: MidiApiContracts.idl
description: API contracts used by the Windows MIDI Services WinRT API namespaces
---

Each Windows MIDI Services WinRT API namespace has its own API contract, and the contract's version goes up when the namespace changes. Your application can check for a contract version with `Windows.Foundation.Metadata.ApiInformation.IsApiContractPresent` before it uses a namespace, so it keeps working on a PC that has an older version of the API.

## Contracts

| Contract | Namespace |
| -------- | --------- |
| `MidiCoreApiContract` | `Windows.Devices.Midi2` |
| `MidiCapabilityInquiryApiContract` | `Windows.Devices.Midi2.CapabilityInquiry` |
| `MidiClientPluginsApiContract` | `Windows.Devices.Midi2.ClientPlugins` |
| `MidiDiagnosticsApiContract` | `Windows.Devices.Midi2.Diagnostics` |
| `MidiEnumerationApiContract` | `Windows.Devices.Midi2.Enumeration` |
| `MidiEnumerationLegacyApiContract` | `Windows.Devices.Midi2.Enumeration.Legacy` |
| `MidiReportingApiContract` | `Windows.Devices.Midi2.Reporting` |
| `MidiServiceConfigApiContract` | `Windows.Devices.Midi2.ServiceConfig` |
| `MidiTransportsLoopbackApiContract` | `Windows.Devices.Midi2.Transports.Loopback` |
| `MidiTransportsBasicLoopbackApiContract` | `Windows.Devices.Midi2.Transports.BasicLoopback` |
| `MidiTransportsVirtualApiContract` | `Windows.Devices.Midi2.Transports.Virtual` |
| `MidiTransportsNetworkApiContract` | `Windows.Devices.Midi2.Transports.Network` |
| `MidiTransportsBluetoothApiContract` | `Windows.Devices.Midi2.Transports.Bluetooth` |
| `MidiTransportsSynthApiContract` | `Windows.Devices.Midi2.Transports.Synth` |
| `MidiTransportsRtpApiContract` | `Windows.Devices.Midi2.Transports.Rtp` |
| `MidiMessageUtilityApiContract` | `Windows.Devices.Midi2.Utilities.Messages` |
| `MidiSysExTransferUtilityApiContract` | `Windows.Devices.Midi2.Utilities.SysExTransfer` |
| `MidiSequencingUtilityApiContract` | `Windows.Devices.Midi2.Utilities.Sequencing` |
| `MidiFilesUtilityApiContract` | `Windows.Devices.Midi2.Utilities.Files` |

## Remarks

All contracts above are at version `1`, except `MidiTransportsRtpApiContract`, `MidiTransportsNetworkApiContract` and `MidiSequencingUtilityApiContract`, which are at version `2`.

- Version 2 of `MidiTransportsRtpApiContract` adds [MidiRtpOfflineRetention]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpOfflineRetentionEnum/), the `OfflineRetentionSeconds` properties that use it, `MidiRtpConfiguredClient.EndpointDeviceId`, and the `SkipInBetweenValuesToCatchUp` properties.
- Version 2 of `MidiTransportsNetworkApiContract` adds the `SkipInBetweenValuesToCatchUp` properties.
- Version 2 of `MidiSequencingUtilityApiContract` adds the `PlaybackRate`, `MinimumPlaybackRate` and `MaximumPlaybackRate` properties to [MidiSequencePlayer]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequencePlayer/), which play a sequence faster or slower without changing its pitch. It also adds the `ChaseOptions` property and [MidiSequenceChaseOptions]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceChaseOptionsEnum/), which say what the player restores when playback starts partway into a sequence.

`SkipInBetweenValuesToCatchUp` turns off skipping in-between values to catch up when messages pile up behind a sending speed. It's on by default. See [Sending speed]({{ site.baseurl }}/kb/network-midi2-transport/#sending-speed).

To check for them, pass the contract's full name and version to `IsApiContractPresent`, for example `ApiInformation.IsApiContractPresent("Windows.Devices.Midi2.Transports.Rtp.MidiTransportsRtpApiContract", 2)`.
