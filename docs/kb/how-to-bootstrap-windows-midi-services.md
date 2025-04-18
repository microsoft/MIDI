---
layout: doc
title: Bootstrap Windows MIDI Services
parent: Developer How-to
has_children: false
---

# How to bootstrap the SDK and Start the Service

TODO: This document is a work-in-progress.

The SDK is a central installation with a bootstrapper COM component. The COM component handles all WinRT activation for Windows MIDI Services without requiring the MIDI types to be included in any type of manifest (for desktop apps). Applications do not deploy anything except their own compiled code.

Individual applications do not deploy the SDK with their binaries. The C# projection in the NuGet package contains everything a C# application needs. For C++, the NuGet package includes a .hpp file with the required implementation.

## Examples

```cpp
// MTA by default
winrt::init_apartment();

// this is the initializer in the bootstrapper hpp file
std::shared_ptr<init::MidiDesktopAppSdkInitializer> initializer = std::make_shared<init::MidiDesktopAppSdkInitializer>();

// you can, of course, use guard macros instead of this check
if (initializer != nullptr)
{
    if (!initializer->InitializeSdkRuntime())
    {
        std::cout << "Could not initialize SDK runtime" << std::endl;
        std::wcout << "Install the latest SDK runtime installer from " << initializer->LatestMidiAppSdkDownloadUrl << std::endl;
        return 1;
    }

    if (!initializer->EnsureServiceAvailable())
    {
        std::cout << "Could not demand-start the MIDI service" << std::endl;
        return 1;
    }
}
else
{
    // This shouldn't happen, but good to guard
    std::cout << "Unable to create initializer" << std::endl;
    return 1;
}
```


```csharp
// the initializer implements the Dispose pattern, so will shut down WinRT type redirection when disposed
using (var initializer = Microsoft.Windows.Devices.Midi2.Initialization.MidiDesktopAppSdkInitializer.Create())
{
    // initialize SDK runtime
    if (!initializer.InitializeSdkRuntime())
    {
        Console.WriteLine("Failed to initialize SDK Runtime");
        return 1;
    }

    // start the service
    if (!initializer.EnsureServiceAvailable())
    {
        Console.WriteLine("Failed to get service running");
        return 1;
    }

    ...

}
```
