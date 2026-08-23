---
layout: kb
title: Developing MIDI Service Transport Plugins (COM)
audience: developers
description: Guidance for creating third-party COM Transport plugins for Windows MIDI Services.
---

Windows MIDI Services Transport plugins are COM components loaded by the MIDI service (`midisrv`). They can often replace custom kernel drivers or legacy WinMM `.drv` style integration for many scenarios, while still allowing a transport to discover, create, and manage endpoints.

This page is a practical starting point for third-party developers who want to build their own transport plugin.

> <h4>Important</h4>
> Transport plugins run in-process inside `midisrv`. Your code executes inside the Windows service process, so reliability, security, and performance requirements are much stricter than for a normal desktop app component. If you have been writing kernel drivers, then you are well-prepared for writing secure and stable user-mode code.

## Overview

At a high level, a transport plugin is:

- A COM in-proc server (DLL)
- Registered with a CLSID
- Listed in the Windows MIDI Services Transport Plugins registry area
- Activated by `midisrv` using COM
- Used to publish and manage MIDI endpoints and to service endpoint I/O

Windows MIDI Services also has a similar plugin mechanism for message transforms. However, discover and runtime activation of transform plugins are not yet enabled for external developers.

## Transport Id and COM identity

Use the same GUID for your transport Id and COM CLSID.

In the current codebase, transports follow this same pattern. For example, `TRANSPORT_LAYER_GUID` is set to the COM CLSID in transport definitions and metadata plumbing.

Examples:

- [Network MIDI 2.0 transport GUID mapping](https://github.com/microsoft/MIDI/blob/main/src/api/Transport/UdpNetworkMidi2Transport/net2udp_transport_defs.h)
- [Network MIDI 2.0 COM class implementation](https://github.com/microsoft/MIDI/blob/main/src/api/Transport/UdpNetworkMidi2Transport/Midi2.NetworkMidiTransport.h)
- [Basic Loopback transport GUID mapping](https://github.com/microsoft/MIDI/blob/main/src/api/Transport/BasicLoopbackMidiTransport/basic_loopback_transport_defs.h)

This one-GUID approach keeps activation, metadata reporting, and configuration routing consistent.

## Data formats

The Windows MIDI Service routes all messages as UMP. However, your transport can expose either UMP or byte format data as appropriate. If you expose MIDI 1.0 byte format data, we will insert a translator in between the rest of the service and your transport.

Transport-facing message format is represented by `MidiDataFormats` in the service interfaces:

- `MidiDataFormats_ByteStream` for MIDI 1.0 byte stream data
- `MidiDataFormats_UMP` for Universal MIDI Packet data (recommended)

Reference:

- [WindowsMidiServices.idl (MidiDataFormats and transport interfaces)](https://github.com/microsoft/MIDI/blob/main/src/api/idl/WindowsMidiServices.idl)

Recommendation:

- Prefer UMP as your primary internal and external representation whenever possible
- Only use byte stream where required for interoperability with legacy devices/protocol paths

## Timestamp format requirements (QPC ticks)

All service transport timestamps are 64-bit ticks based on `QueryPerformanceCounter` (QPC), relative to PC startup time.

The `position`/timestamp fields passed through service transport interfaces are not wall-clock time and not MIDI beat time. They are high-resolution performance counter ticks.

References:

- [WindowsMidiServices.idl (timestamp semantics in callback and transport comments)](https://github.com/microsoft/MIDI/blob/main/src/api/idl/WindowsMidiServices.idl)
- [midi_timestamp.h (QPC helper functions)](https://github.com/microsoft/MIDI/blob/main/src/api/Inc/midi_timestamp.h)

Guidance:

- Convert your source timestamps into QPC tick space before submitting to the service
- Prefer transport-supplied, closer-to-the-wire timestamps when possible
- A value of `0` is allowed and indicates "timestamp not supplied"; the service pipeline can stamp later, but this is less accurate than a transport-origin timestamp

## Sending and receiving messages with IMidiBidirectional

For UMP endpoints, `IMidiBidirectional` is the main data path for both directions. In this, you want to quickly process incoming messages from the service, and quickly send messages going into the service. Typically, this class is adding to or pulling from an internal queue of message data in the transport.

Relevant interfaces and methods:

- `IMidiBidirectional::SendMidiMessage(...)` for transport-to-service message flow
- `IMidiCallback::Callback(...)` for service-to-transport callback message flow

Reference:

- [WindowsMidiServices.idl (IMidiBidirectional and IMidiCallback)](https://github.com/microsoft/MIDI/blob/main/src/api/idl/WindowsMidiServices.idl)

### Multiple messages per call

Both directions can carry more than one UMP message in a single call.

Guidance for transport developers:

- You will receive messages in the format you declared up-front
- Do not assume one call equals one MIDI message
- Parse the entire message buffer and process each contained UMP message in order
- Preserve ordering for all messages in a call

You can use either approach:

- Use a copy of the in-repo UMP iterator helper: [ump_iterator.h](https://github.com/microsoft/MIDI/blob/main/src/api/Inc/ump_iterator.h)
- Implement your own parser/iterator if it is more appropriate for your codebase

### Message grouping intent

When multiple messages are sent from the transport to the service in a single call, the service attempts to preserve that grouping together to clients as much as possible.

Likewise, when multiple messages are delivered to the transport in one callback call, that call represents client intent that those messages be sent together.

This means your implementation should avoid splitting or re-chunking within a single call unless required by a strict transport/protocol constraint.

## Core service-facing components

Your transport exposes functionality through COM interfaces activated from the root transport object.

Primary interfaces include:

- `IMidiTransport` (root activation interface)
- `IMidiEndpointManager` (endpoint discovery/publication lifecycle)
- `IMidiTransportConfigurationManager` (service/app-driven transport config updates)
- `IMidiServiceTransportPluginMetadataProvider` (name, author, version, flags, etc.)
- `IMidiIn`, `IMidiOut`, `IMidiBidirectional` (data path instances)

References:

- [WindowsMidiServices.idl (transport, endpoint manager, configuration, metadata)](https://github.com/microsoft/MIDI/blob/main/src/api/idl/WindowsMidiServices.idl)
- [Basic Loopback transport Activate pattern](https://github.com/microsoft/MIDI/blob/main/src/api/Transport/BasicLoopbackMidiTransport/Midi2.BasicLoopbackMidiTransport.cpp)
- [Network transport Activate pattern](https://github.com/microsoft/MIDI/blob/main/src/api/Transport/UdpNetworkMidi2Transport/Midi2.NetworkMidiTransport.cpp)

### About the “Transport Factory”

`IMidiTransport::Activate` is your transport's factory for all the additional types. Inside this, COM class factory activation is used (`CoCreateInstance` on your CLSID), then interface activation is handled through `IMidiTransport::Activate`.

## Creating UMP endpoints and MIDI 1.0 ports

By default, design for UMP endpoint creation first.

For endpoint publication, your `IMidiEndpointManager` implementation creates and manages Software Device endpoints and their associated metadata/properties.

References:

- [Network MIDI Endpoint Manager](https://github.com/microsoft/MIDI/blob/main/src/api/Transport/UdpNetworkMidi2Transport/Midi2.NetworkMidiEndpointManager.cpp)
- [Basic Loopback Endpoint Manager](https://github.com/microsoft/MIDI/blob/main/src/api/Transport/BasicLoopbackMidiTransport/Midi2.BasicLoopbackMidiEndpointManager.cpp)

If your transport needs to expose legacy MIDI 1.0 ports in addition to UMP endpoints, provide an explicit configuration switch and keep the behavior deterministic.

The Network MIDI 2.0 transport shows this pattern:

- `createMidi1Ports` controls whether MIDI 1.0 ports are also created
- Default behavior is UMP-focused

References:

- [Network config key `createMidi1Ports`](https://github.com/microsoft/MIDI/blob/main/src/api/Transport/UdpNetworkMidi2Transport/network_json_defs.h)
- [Network configuration processing (`UpdateConfiguration`)](https://github.com/microsoft/MIDI/blob/main/src/api/Transport/UdpNetworkMidi2Transport/Midi2.NetworkMidiConfigurationManager.cpp)
- [Network host/client endpoint mode selection](https://github.com/microsoft/MIDI/blob/main/src/api/Transport/UdpNetworkMidi2Transport/MidiNetworkHost.cpp)

## Midisrv is the only client

For service transport plugin interfaces, `midisrv` is your only caller.

The service:

- Reads enabled transport CLSIDs from registry
- Activates transports by CLSID
- Activates endpoint/config/metadata interfaces
- Initializes managers and endpoint communication flows

References:

- [Transport registry enumeration and CLSID parsing](https://github.com/microsoft/MIDI/blob/main/src/api/Service/Exe/MidiConfigurationManager.cpp)
- [Transport activation and manager initialization](https://github.com/microsoft/MIDI/blob/main/src/api/Service/Exe/MidiDeviceManager.cpp)

## Versioning and compatibility contract

Plan for mixed-version environments where your transport, the service, and client-facing tooling may not all update at the same time.

Recommendations:

- Treat transport configuration JSON as versioned input and tolerate unknown properties
- Use capability checks instead of hard assumptions when enabling optional behaviors
- Return a clear error for unsupported verbs or commands instead of failing silently
- Keep transport IDs, command verbs, and stable response fields backward-compatible once published

References:

- [MidiServiceTransportPluginConfigManager.idl](https://github.com/microsoft/MIDI/blob/main/src/api/Client/WinRT/t/core/MidiServiceTransportPluginConfigManager.idl)
- [MidiServiceTransportCommonCommands.idl](https://github.com/microsoft/MIDI/blob/main/src/api/Client/WinRT/t/core/MidiServiceTransportCommonCommands.idl)

## Required registry entries

At minimum, you need:

1. COM registration for your CLSID (`InprocServer32`, threading model, etc.)
2. A Windows MIDI Services transport plugin entry that points to that CLSID

Windows MIDI Services constants define these values:

- Root key: `HKLM\Software\Microsoft\Windows MIDI Services\Transport Plugins`
- Per-plugin values:
- `CLSID` (string GUID, required)
- `Enabled` (DWORD, optional; missing means enabled, as does a value of `1`)

References:

- [Registry key/value constants](https://github.com/microsoft/MIDI/blob/main/src/api/Inc/MidiDefs.h)
- [How service reads Enabled and CLSID](https://github.com/microsoft/MIDI/blob/main/src/api/Service/Exe/MidiConfigurationManager.cpp)

Illustrative example:

```reg
[HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows MIDI Services\Transport Plugins\MyCompany.MyTransport]
"CLSID"="{12345678-1234-1234-1234-1234567890AB}"
"Enabled"=dword:00000001
```

## Code signing and Developer Mode

Service plugins are subject to a signing check before loading.

For development and local testing, enabling Developer Mode in Windows Settings bypasses the plugin signing requirement. This is useful for unsigned test builds, but it should not be used as a production deployment model.

Recommendations:

- Use trusted EV-based code signing for production plugin binaries
- Use Developer Mode bypass only on trusted development/test machines
- Re-test with production-style signing before release

Reference:

- [Why a Service Plugin may not load correctly](service-plugin-not-loaded)

## Recommended installation location

Install transport binaries in `Program Files` and register from that stable location. 32-bit transports are not supported.

Recommendations:

- Do not load plugin binaries from user-writable folders
- Support clean upgrade/uninstall behavior

## Deployment and servicing

Treat deployment and servicing as part of your transport design, not an afterthought.

Recommendations:

- Keep installer behavior idempotent so repair/reinstall does not create duplicate plugin entries
- On upgrade, preserve stable identifiers and migrate configuration data carefully
- On uninstall, remove plugin registration cleanly and avoid leaving orphaned service plugin entries
- Validate rollback behavior so the service starts cleanly with either old or new transport versions
- Test update paths with active endpoints, not just clean-boot scenarios

## Security guidance

Your plugin must be safe to run under the service account context (Local Service) and must treat all external data as untrusted.

Recommendations:

- Assume least privilege and restricted token behavior
- Do not require interactive desktop access
- Avoid direct arbitrary file read/write at runtime
- Avoid dynamic code execution patterns
- Validate and bounds-check all incoming data (network, device, JSON, registry)
- Fail safely with explicit error returns

For signing policy and development-mode bypass guidance, see the Code signing and Developer Mode section above.

Reference:

- [Why a Service Plugin may not load correctly](service-plugin-not-loaded)

## Threading model and reentrancy

Assume callbacks and control operations can occur on different threads, and design for reentrancy.

Recommendations:

- Keep callback paths short, non-blocking, and allocation-aware
- Avoid lock inversion between data-path callbacks and control/configuration paths
- Prefer single-responsibility worker queues for protocol/network/device work that may block
- Define lock ordering explicitly and enforce it consistently
- Make shutdown coordination explicit so worker threads drain and stop deterministically

Operational expectation:

- Your transport should continue to behave correctly if callbacks arrive while control operations are in progress

## Error contract and response patterns

Use a clear split between COM boundary failures and transport-domain failures.

Recommendations:

- Use `HRESULT` for interface activation and contract-level failures (invalid args, unavailable interfaces, initialization failure)
- Use structured service response payloads for transport-domain command/config failures where a detailed reason helps the caller recover
- Keep error codes stable and documented once you expose them to external developers
- Include actionable, non-sensitive text in error messages
- Do not leak internal-only state or secrets in failure paths

Reference examples:

- [MidiServiceConfigResponse.idl](https://github.com/microsoft/MIDI/blob/main/src/api/Client/WinRT/t/core/MidiServiceConfigResponse.idl)
- [basic_loopback_transport_error_codes.h](https://github.com/microsoft/MIDI/blob/main/src/api/Transport/BasicLoopbackMidiTransport/basic_loopback_transport_error_codes.h)

## Lifecycle state model

Define and test an explicit lifecycle model for each major transport component.

Typical service flow for transport components:

1. COM activation of transport root object
2. Interface activation through `IMidiTransport::Activate`
3. `Initialize` for endpoint/configuration managers and data-path objects
4. Runtime operation with config updates and command processing
5. `Shutdown` and resource teardown

Recommendations:

- Make repeated `Initialize`/`Shutdown` calls safe and predictable
- Reject out-of-order operations with clear errors
- Ensure partial initialization failures unwind cleanly
- Keep ownership/lifetime rules obvious between root transport, managers, and workers

Reference:

- [WindowsMidiServices.idl](https://github.com/microsoft/MIDI/blob/main/src/api/idl/WindowsMidiServices.idl)

## Stability guidance

Transport plugins run in-proc with `midisrv`, so failures can impact the entire MIDI stack on the machine.

Minimum stability bar:

- No exception leakage across COM boundaries
- No deadlocks in callbacks, worker shutdown, or endpoint teardown
- No unbounded blocking on the service main/control paths
- No memory/resource leaks (handles, threads, COM refs, sockets, watchers)
- Predictable shutdown and restart behavior

Recommended patterns:

- Catch all exceptions at API boundaries and convert to `HRESULT` or structured response
- Use dedicated worker queues/threads for transport operations that can block
- Keep lock scopes short and avoid lock inversion between callback and control paths
- Support idempotent cleanup (`Initialize`/`Shutdown` ordering should be robust)

## Framework and language choices

The in-box transports use ATL COM, which is consistent with other similar Windows components.

You are not required to use ATL specifically, but your implementation should produce high-quality native COM components with predictable lifetime and threading behavior.

Recommendations:

- Prefer native languages and deterministic resource management
- Avoid managed runtimes or garbage-collected environments in this in-proc service plugin path
- Avoid anything that significantly increases `midisrv` memory footprint or introduces non-deterministic pauses

## Pattern for user intervention and approval

For scenarios where user approval is required (for example, Network MIDI 2.0 remote client approval), use a split-control pattern:

1. Transport reports pending state and required context
2. A trusted app/tool surfaces the decision to the user
3. App/tool sends an explicit approve/deny command back through service config APIs
4. Transport updates policy/state and returns structured result

This keeps policy decisions outside the service UI surface while preserving a service-controlled source of truth.

References:

- [Network setup tool approval flow](https://github.com/microsoft/MIDI/blob/main/src/api/Client/WinRT/user-tools/network-midi-setup/MainWindowActions.cpp)
- [Network transport manager command/config flow](https://github.com/microsoft/MIDI/blob/main/src/api/Client/WinRT/t/core/MidiNetworkTransportManager.cpp)
- [Network approval config type](https://github.com/microsoft/MIDI/blob/main/src/api/Client/WinRT/t/core/MidiNetworkRemoteClientApprovalConfig.idl)

## Diagnostics and supportability

Plan diagnostics early so production issues can be triaged without code changes.

Recommendations:

- Use structured logging with stable event names and fields for lifecycle, connection, and configuration events
- Include identifiers useful for support, such as transport ID, endpoint ID, device/port names, and operation/result codes
- Include correlation IDs or operation IDs for multi-step workflows
- Log enough context to reproduce failures without logging sensitive or high-volume payloads

Important data-handling guidance:

- Microsoft does not log actual customer MIDI message payload data in production for privacy and performance reasons
- Device names, endpoint names, and port metadata are generally acceptable when needed for diagnostics
- Third-party plugin authors should follow the same principle and avoid payload logging by default

## WinRT Service Transport Config Manager example

The WinRT API provides a straightforward way to send custom transport JSON, invoke command verbs, and query capabilities.

Key API surface:

- [IMidiServiceTransportPluginConfig](https://github.com/microsoft/MIDI/blob/main/src/api/Client/WinRT/t/core/IMidiServiceTransportPluginConfig.idl)
- [MidiServiceTransportPluginConfigManager](https://github.com/microsoft/MIDI/blob/main/src/api/Client/WinRT/t/core/MidiServiceTransportPluginConfigManager.idl)
- [MidiServiceTransportCommand](https://github.com/microsoft/MIDI/blob/main/src/api/Client/WinRT/t/core/MidiServiceTransportCommand.idl)
- [MidiServiceTransportCommonCommands](https://github.com/microsoft/MIDI/blob/main/src/api/Client/WinRT/t/core/MidiServiceTransportCommonCommands.idl)
- [MidiServiceConfigResponse](https://github.com/microsoft/MIDI/blob/main/src/api/Client/WinRT/t/core/MidiServiceConfigResponse.idl)

Example (C++/WinRT):

```cpp
using namespace winrt;
using namespace Windows::Data::Json;
using namespace Windows::Devices::Midi2::ServiceConfig;

void ConfigureTransport(guid transportId)
{
    JsonObject update;
    update.Insert(L"action", JsonValue::CreateStringValue(L"refresh"));
    update.Insert(L"reason", JsonValue::CreateStringValue(L"user-request"));

    auto response = MidiServiceTransportPluginConfigManager::SendUpdate(transportId, update);

    if (response.Status() == MidiServiceConfigResponseStatus::Success)
    {
        // Transport-defined JSON payload
        auto result = response.ResponseJson();
    }
    else
    {
        auto serviceCode = response.ServiceErrorCode();
        auto serviceMessage = response.ServiceErrorMessage();
        // Log/report serviceCode + serviceMessage
    }

    // Query a single capability key
    bool supportsFeatureX = MidiServiceTransportPluginConfigManager::QueryCapability(
        transportId,
        L"supportsFeatureX");

    // Query all capability flags
    auto allCaps = MidiServiceTransportPluginConfigManager::QueryAllCapabilities(transportId);

    // Send a command verb
    MidiServiceTransportCommand cmd(transportId);
    cmd.Verb(MidiServiceTransportCommonCommands::QueryCapabilities());

    auto cmdResponse = MidiServiceTransportPluginConfigManager::SendCommand(cmd);
}
```

For real usage examples in this repo, see:

- [Transport config manager implementation](https://github.com/microsoft/MIDI/blob/main/src/api/Client/WinRT/t/core/MidiServiceTransportPluginConfigManager.cpp)
- [Basic Loopback runtime config/update usage](https://github.com/microsoft/MIDI/blob/main/src/api/Client/WinRT/t/core/MidiBasicLoopbackManager.cpp)
- [Network transport command usage](https://github.com/microsoft/MIDI/blob/main/src/api/Client/WinRT/t/core/MidiNetworkTransportManager.cpp)

## Existing transport examples in this repo

These built-in transports are good reference points when creating your own COM transport plugin:

- [UdpNetworkMidi2Transport](https://github.com/microsoft/MIDI/tree/main/src/api/Transport/UdpNetworkMidi2Transport)
- [KSTransport](https://github.com/microsoft/MIDI/tree/main/src/api/Transport/KSTransport)
- [BasicLoopbackMidiTransport](https://github.com/microsoft/MIDI/tree/main/src/api/Transport/BasicLoopbackMidiTransport)
- [LoopbackMidiTransport](https://github.com/microsoft/MIDI/tree/main/src/api/Transport/LoopbackMidiTransport)

These examples show how to structure COM activation, endpoint/config managers, metadata reporting, and transport lifecycle behavior.

The Network MIDI 2.0 transport shows how to interact with an external data source/destination and protocol from user mode. The KS Transport interacts with the kernel mode UMP driver. Basic Loopback and Loopback show implementing a transport which simply routes messages internally, without any external hooks or protocol involved.

> When you look at the example code, you may find `if` statements that include a flag check with a name like `FeatureServicing_XYZ`. Those are for internal Windows CFR rollouts. In the public repo, those all resolve to `true`. Internally, there are more checks for feature enablement. Over time, the `else` branch of those checks gets pruned from the source.

## Final checklist

Before shipping a third-party transport plugin, verify:

- CLSID equals transport Id in your plugin model
- COM registration is correct and points to your installed DLL
- Transport plugin registry entry exists with valid `CLSID`
- Plugin can initialize under service context without user profile assumptions
- Endpoint manager and config manager handle malformed input safely
- No unhandled exceptions cross COM boundaries
- Shutdown is clean and repeatable
- UMP path is validated and tested for high-throughput/low-latency use
