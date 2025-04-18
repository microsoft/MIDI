---
layout: home
---
# About Windows MIDI Services

Windows MIDI Services is the new MIDI API, Service, and SDK in Microsoft Windows. It's delivered in two parts:

- In-box MIDI Service and plugins, the new MIDI 2.0 kernel driver, plus backwards-compatibility support for WinMM and WinRT MIDI 1.0 APIs. You get this when you install Windows.
- An out-of-band shipped SDK Runtime and Tools package. You need to download and install this yourself.

We ship the runtime and tools out-of-band to enable us to move quickly and continue providing new features and capabilities for customers and apps.

## Benefits for everyone

* **No need for third-party USB drivers in most cases**. Most USB MIDI 1.0 devices are class-compliant and will work without third-party drivers. This means no wrestling with their position in the registry, or worrying about compatibility. Don't install third-party drivers unless absolutely necessary, and you'll have a better MIDI experience on Windows.
* **Supports your Existing Apps**. The existing WinMM and WinRT MIDI 1.0 APIs have been repointed to the new Windows Service. This provides a subset of the new features, including multi-client so your apps will continue to work as today, but will be even better.
* **Multi-client by default**. Any endpoint (including MIDI 1.0 devices) can be used by multiple applications at the same time. That means you can use a librarian or controller app at the same time your DAW has a connection open.
* **Supports your Existing Devices**. Windows MIDI Services supports the MIDI 1.0 devices you own today, including those with vendor-supplied kernel streaming drivers, as well as class-compliant MIDI 1.0 and MIDI 2.0 devices. The experience will be better/faster if they use the new class driver, but we recognize that is not always possible or desirable with some existing devices.
* **Faster**. In our testing, we've found that the new infrastructure is much faster at sending and receiving messages compared to the older API, even with plugins configured in the service. There are no built-in speed caps or throttling in Windows MIDI Services, even for older USB MIDI 1.0 devices. The new MIDI 2.0 driver is not limited USB full-speed, and supports USB 3.x speeds.
* **Lower Jitter**. Along with higher speed comes lower jitter. This will vary by transport type (USB vs Network vs Virtual), and the device Windows is talking to, but the jitter is in the low microsecond range even without any compensation.
* **More Deterministic**. Speaking of latency compensation, the new API enables timestamp-based message scheduling for outbound messages for any apps using the new API. In addition, incoming messages are tagged with a timestamp when received by the service.
* **App-to-App and Virtual Device MIDI**. Windows MIDI Services includes built-in virtual / app-to-app MIDI 2.0 to enable lightning fast communication between apps on the PC.
* **Better tools**. We supply the `midi.exe` Windows MIDI Services Console for developers and power users, or anyone comfortable with the command line. You can use it to monitor endpoints, send and receive messages, send/capture SysEx data and much more. We also include the MIDI Settings GUI app for renaming devices, configuring your MIDI setup, testing, and more.
* **Built-in Scripting** Built-in support for scripting MIDI using PowerShell. Want to automate synchronization between mixers? Want to set up a script to initialize all your devices for a show? All this can be done via the PowerShell Cmdlets.

## Developer Benefits

* **UMP-Centric**. The new SDK fully embraces MIDI 2.0 and the Universal MIDI Packet format and handles all required translation in the service and driver. This makes the app model simple while ensuring all your existing devices continue to work. Messages to and from any endpoint, whether it is MIDI 1.0 or MIDI 2.0 data format, are transparently translated in the service, and presented as UMP through the SDK.
* **Extensive Endpoint Metadata** We've built upon `Windows.Devices.Enumeration` with our own MIDI-specific device class, which provides much more information about a device. Not only do we have user-provided metadata (name, description, images, settings), but also more device-provided information. This is also where you have access to MIDI 2.0 device features like Function Blocks, Group Terminal Blocks, Device Identity, and much more. See [MidiEndpointDeviceInformation](/sdk-reference/MidiEndpointDeviceInformation/) for details.
* **Device Connect/Disconnect Notification** No restarting your apps to detect the presence of a new device. Whenever an endpoint is created, updated, or removed, the app receives an event with all the required details. Additionally, apps can opt-in to automatic reconnect to any device they are communicating with, should it become disconnected during a session. See [MidiEndpointDeviceWatcher](/sdk-reference/MidiEndpointDeviceWatcher/) for details.
* **Extensible**. The service has been designed to be extensible by Microsoft and third-parties. New types of transports can be added at any time, including during prototyping of a new transport specification. (We're working on Network MIDI 2.0, Bluetooth MIDI 1.0 and considering RTP, all using this model.) **No kernel driver experience required in most cases.**
* **Open Source**. The source code is open and available to everyone under a permissive license. Not sure how something works? Want to create a transport but aren't sure how we did it? Want to investigate a bug or contribute a feature? The code is there for you to explore.
