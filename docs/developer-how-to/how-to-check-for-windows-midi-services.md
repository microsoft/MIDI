---
layout: page
title: Check for Windows MIDI Services
parent: Developer How-to
has_children: false
---

# How to check for Windows MIDI Services on the PC

TODO: Explain why we chose this model



## Verify that the OS is supported 

Windows MIDI Services runs on the latest Windows 10 public release, as well as all supported versions of Windows 11. 

```cpp
if (MidiServicesInitializer::IsOperatingSystemSupported())
{
    // continue with other checking
}
else
{
    // fallback to older MIDI APIs
}
```

## Verify that the Windows Service is installed and running

The Windows Service (midisrv.exe) used for Windows MIDI Services is, by Microsoft policy, set to demand-start by default, so that it does not impact Windows start-up performance. (Any customer is free to change it to start automatically should they wish.) To ensure the service is available and started, call the `EnsureServiceAvailable` function

```cpp
if (MidiServicesInitializer::EnsureServiceAvailable())
{
    // MIDI Service is now available. If it was not previously started, 
    // Devices enumeration and protocol discovery/negotiation will now
    // begin. 
}
else
{
    // service is not installed or otherwise couldn't start
}

```

### How this works

The Windows Service is triggered to start via `EnsureServiceAvailable()`, which calls the service interface and triggers an ETL event. The ETL event is what the service watches for to spin up. Typically, this takes only a second or two to happen.

Once the service hass started, all the devices it is responsible for begin to be enumerated. It will remain running until manually shut down or the PC is rebooted.

> Musicians may want to set the service to auto-start with Windows. It adds a little bit of time to Windows startup, but the devices will be available when needed.

## Bootstrap the Windows MIDI Services SDK runtime (Desktop apps only)

For desktop apps, other than the initializer, the rest of the SDK is installed centrally on the PC rather than shipped with applications. This enables bug and security fixes without asking each software developer to ship a new version of their application.

Because the SDK is shipped out-of-band from Windows itself, and is restricted to specific versions of Windows and devices (no current support on Xbox and Hololens, for example) the SDK must be bootstrapped/initialized so the application can find the WinRT types contained within.

Internally, the initializer uses a combination of Registration-free WinRT and the Detours library to hook into type resolution and activation. To support the use of the initializer, the application must include an entry in the application manifest file, named `AppName.exe.manifest` where "AppName" is the name of the executable.

Manifest contents:

```xml
<?xml version="1.0" encoding="utf-8"?>
<assembly manifestVersion="1.0" xmlns="urn:schemas-microsoft-com:asm.v1">
    <assemblyIdentity version="1.0.0.0" name="MyWindowsMidiServicesApplication.app"/>
    <!-- This is the only manifest entry needed for desktop apps using MIDI -->

    <!-- This specific DLL is shipped with the app itself -->
    <file name="Microsoft.Windows.Devices.Midi2.Initialization.dll">
        <activatableClass
            name="Microsoft.Windows.Devices.Midi2.Initialization.IMidiServicesInitializerStatics"
            threadingModel="both"
            xmlns="urn:schemas-microsoft-com:winrt.v1" />
    </file>
    <!-- Other manifest entries are not impacted by this entry -->
</assembly>
```

The manifest may contain other information unrelated to Windows MIDI Services. More details about application manifests may be found on the [Microsoft Learn documentation site](https://learn.microsoft.com/windows/win32/sbscs/application-manifests).

In addition to the manifest, the application must include the `Microsoft.Windows.Devices.Midi2.Initialization.dll` and `Microsoft.Windows.Devices.Midi2.Initialization.winmd` files in its application folder, side-by-side with the executable.

### What to do if the runtime is not present

If the runtime is not present, but the service is present, the application can either prompt the user to download and install the runtime. That is an application-specific decision.

```cpp
auto uri = MidiServicesInitializer::GetLatestRuntimeReleaseInstallerUri();
```

> Your desktop application's installer can also include code to download and install the latest Windows MIDI Services runtime, rather than doing this after the application has already started.

## Use the SDK from packaged (Store etc.) apps

If the app is packaged using MSIX, the bootstrapper is not used. Instead, the app must declare all Windows MIDI Services WinRT types in its manifest. It must also declare a dependency on ... 

TODO

## Sample Code

The existing samples are in the process of being updated with this new bootstrapping code.
