// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code

#include <iostream>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Microsoft.Windows.Devices.Midi2.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Diagnostics.h>

using namespace winrt::Microsoft::Windows::Devices::Midi2;                  // SDK Core
using namespace winrt::Microsoft::Windows::Devices::Midi2::Diagnostics;     // For diagnostics loopback endpoints


// where you find types like IAsyncOperation, IInspectable, etc.
namespace foundation = winrt::Windows::Foundation;

// This include file has a wrapper for the bootstrapper code. You are welcome to
// use the .hpp as-is, or work the functionality into your code in whatever way
// makes the most sense for your application.
// 
// The namespace defined in the .hpp is not a WinRT namespace, just a regular C++ namespace
#include "winmidi/init/Microsoft.Windows.Devices.Midi2.Initialization.hpp"
namespace init = Microsoft::Windows::Devices::Midi2::Initialization;



std::string BooleanToString(bool value)
{
    if (value)
        return "true";
    else
        return "false";
}

int main()
{
    // initialize the thread before calling the bootstrapper or any WinRT code. You may also
    // be able to leave this out and call RoInitialize() or CoInitializeEx() before creating
    // the initializer.
    //winrt::init_apartment(winrt::apartment_type::single_threaded);

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



    bool includeDiagnosticsEndpoints = true;

    std::cout << "Enumerating endpoints..." << std::endl;

    auto filter = 
        MidiEndpointDeviceInformationFilters::StandardNativeMidi1ByteFormat |
        MidiEndpointDeviceInformationFilters::StandardNativeUniversalMidiPacketFormat |
        MidiEndpointDeviceInformationFilters::VirtualDeviceResponder;   // apps normally don't use this type

    // normally, applications should not include diagnostics endpoints unless they are
    // providing diagnostics functions.
    if (includeDiagnosticsEndpoints)
    {
        filter |= MidiEndpointDeviceInformationFilters::DiagnosticLoopback;
    }

    auto watcher = MidiEndpointDeviceWatcher::Create(filter);


    // if the watcher's Stop method is called
    auto OnWatcherStopped = [&](MidiEndpointDeviceWatcher const& /*sender*/, foundation::IInspectable const& /*args*/)
        {
            std::cout << std::endl;
            std::cout << "Watcher stopped." << std::endl;
        };

    // called when initial enumeration has completed. Endpoints can appear or disappear at any time afterwards. For example,
    // a user can plug in a USB device after enumeration has completed.
    auto OnWatcherEnumerationCompleted = [&](MidiEndpointDeviceWatcher const& /*sender*/, foundation::IInspectable const& /*args*/)
        {
            std::cout << std::endl;
            std::cout << "Initial enumeration completed." << std::endl;
        };


    // Endpoints can be removed at any time, including after enumeration has completed. This event will fire when they are removed
    auto OnWatcherDeviceRemoved = [&](MidiEndpointDeviceWatcher const& /*sender*/, MidiEndpointDeviceInformationRemovedEventArgs const& args)
        {
            std::cout << std::endl;
            std::cout << "Removed: " << winrt::to_string(args.EndpointDeviceId()) << std::endl;
        };

    // Endpoints can be updated at any time, typically due to protocol negotiation, discovery, or changes the user has made in the settings app
    auto OnWatcherDeviceUpdated = [&](MidiEndpointDeviceWatcher const& /*sender*/, MidiEndpointDeviceInformationUpdatedEventArgs const& args)
        {
            std::cout << std::endl;
            std::cout << "Updated: " << winrt::to_string(args.EndpointDeviceId()) << std::endl;

            // Show how to use the various data update flags here

            if (args.IsNameUpdated())                     std::cout << "- Name" << std::endl;
            if (args.IsUserMetadataUpdated())             std::cout << "- User Metadata" << std::endl;
            if (args.IsEndpointInformationUpdated())      std::cout << "- Endpoint Information" << std::endl;
            if (args.IsStreamConfigurationUpdated())      std::cout << "- Stream Configuration" << std::endl;
            if (args.AreFunctionBlocksUpdated())          std::cout << "- Function Blocks" << std::endl;
            if (args.IsDeviceIdentityUpdated())           std::cout << "- Device Identity" << std::endl;
            if (args.AreAdditionalCapabilitiesUpdated())  std::cout << "- Additional Capabilities" << std::endl;

        };

    // During initial enumeration, this event fires for each endpoint found. After initial enumeration, this will fire
    // for any new endpoints added (examples: USB Device plugged in, Virtual Device created by an app, etc.
    auto OnWatcherDeviceAdded = [&](MidiEndpointDeviceWatcher const& /*sender*/, MidiEndpointDeviceInformationAddedEventArgs const& args)
        {
            std::cout << std::endl;
            std::cout << "Added  : " << winrt::to_string(args.AddedDevice().Name()) << std::endl;
            std::cout << "         " << winrt::to_string(args.AddedDevice().EndpointDeviceId()) << std::endl ;
        };


    // these are each winrt::event_token, which can be checked like a boolean for validity
    auto revokeOnWatcherStopped = watcher.Stopped(OnWatcherStopped);
    auto revokeOnWatcherEnumerationCompleted = watcher.EnumerationCompleted(OnWatcherEnumerationCompleted);
    auto revokeOnWatcherDeviceRemoved = watcher.Removed(OnWatcherDeviceRemoved);
    auto revokeOnWatcherDeviceUpdated = watcher.Updated(OnWatcherDeviceUpdated);
    auto revokeOnWatcherDeviceAdded = watcher.Added(OnWatcherDeviceAdded);

    watcher.Start();

    std::cout << std::endl << "Plug and unplug devices, update properties etc. Press any key when you have finished." << std::endl << std::endl;

    system("pause");

    watcher.Stop();

    // unwire events

    if (revokeOnWatcherStopped) watcher.Stopped(revokeOnWatcherStopped);
    if (revokeOnWatcherEnumerationCompleted) watcher.EnumerationCompleted(revokeOnWatcherEnumerationCompleted);
    if (revokeOnWatcherDeviceRemoved) watcher.Removed(revokeOnWatcherDeviceRemoved);
    if (revokeOnWatcherDeviceUpdated) watcher.Updated(revokeOnWatcherDeviceUpdated);
    if (revokeOnWatcherDeviceAdded) watcher.Added(revokeOnWatcherDeviceAdded);


    // clean up the SDK WinRT redirection
    if (initializer != nullptr)
    {
        initializer->ShutdownSdkRuntime();
        initializer.reset();
    }
}
