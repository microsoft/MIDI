// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code

#include <iostream>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>
#include <winrt/Windows.Devices.Midi2.Diagnostics.h>

using namespace winrt::Windows::Devices::Midi2;                  // SDK Core
using namespace winrt::Windows::Devices::Midi2::Enumeration;     // Enumeration
using namespace winrt::Windows::Devices::Midi2::Diagnostics;     // For diagnostics loopback endpoints


// where you find types like IAsyncOperation, IInspectable, etc.
namespace foundation = winrt::Windows::Foundation;


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

    if (!MidiApi::EnsureServiceAvailable())
    {
        std::wcout << L"Could not demand-start the MIDI service" << std::endl;
        return 1;
    }


    bool includeDiagnosticsEndpoints = true;

    std::wcout << L"Enumerating endpoints..." << std::endl;

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
            std::wcout << std::endl;
            std::wcout << L"Watcher stopped." << std::endl;
        };

    // called when initial enumeration has completed. Endpoints can appear or disappear at any time afterwards. For example,
    // a user can plug in a USB device after enumeration has completed.
    auto OnWatcherEnumerationCompleted = [&](MidiEndpointDeviceWatcher const& /*sender*/, foundation::IInspectable const& /*args*/)
        {
            std::wcout << std::endl;
            std::wcout << L"Initial enumeration completed." << std::endl;
        };


    // Endpoints can be removed at any time, including after enumeration has completed. This event will fire when they are removed
    auto OnWatcherDeviceRemoved = [&](MidiEndpointDeviceWatcher const& /*sender*/, MidiEndpointDeviceInformationRemovedEventArgs const& args)
        {
            std::wcout << std::endl;
            std::wcout << L"Removed: " << args.EndpointDeviceId().c_str() << std::endl;
        };

    // Endpoints can be updated at any time, typically due to protocol negotiation, discovery, or changes the user has made in the settings app
    auto OnWatcherDeviceUpdated = [&](MidiEndpointDeviceWatcher const& /*sender*/, MidiEndpointDeviceInformationUpdatedEventArgs const& args)
        {
            std::wcout << std::endl;
            std::wcout << "Updated: " << args.EndpointDeviceId().c_str() << std::endl;

            // Show how to use the various data update flags here

            if (args.IsNameUpdated())                     std::wcout << L"- Name" << std::endl;
            if (args.IsUserMetadataUpdated())             std::wcout << L"- User Metadata" << std::endl;
            if (args.IsEndpointInformationUpdated())      std::wcout << L"- Endpoint Information" << std::endl;
            if (args.IsStreamConfigurationUpdated())      std::wcout << L"- Stream Configuration" << std::endl;
            if (args.AreFunctionBlocksUpdated())          std::wcout << L"- Function Blocks" << std::endl;
            if (args.IsDeviceIdentityUpdated())           std::wcout << L"- Device Identity" << std::endl;
            if (args.AreAdditionalCapabilitiesUpdated())  std::wcout << L"- Additional Capabilities" << std::endl;

        };

    // During initial enumeration, this event fires for each endpoint found. After initial enumeration, this will fire
    // for any new endpoints added (examples: USB Device plugged in, Virtual Device created by an app, etc.
    auto OnWatcherDeviceAdded = [&](MidiEndpointDeviceWatcher const& /*sender*/, MidiEndpointDeviceInformationAddedEventArgs const& args)
        {
            std::wcout << std::endl;
            std::wcout << "Added  : " << args.AddedDevice().Name().c_str() << std::endl;
            std::wcout << "         " << args.AddedDevice().EndpointDeviceId().c_str() << std::endl ;
        };


    // these are each winrt::event_token, which can be checked like a boolean for validity
    auto revokeOnWatcherStopped = watcher.Stopped(OnWatcherStopped);
    auto revokeOnWatcherEnumerationCompleted = watcher.EnumerationCompleted(OnWatcherEnumerationCompleted);
    auto revokeOnWatcherDeviceRemoved = watcher.Removed(OnWatcherDeviceRemoved);
    auto revokeOnWatcherDeviceUpdated = watcher.Updated(OnWatcherDeviceUpdated);
    auto revokeOnWatcherDeviceAdded = watcher.Added(OnWatcherDeviceAdded);

    watcher.Start();

    std::wcout << std::endl << L"Plug and unplug devices, update properties etc. Press any key when you have finished." << std::endl << std::endl;

    system("pause");

    watcher.Stop();

    // unwire events

    if (revokeOnWatcherStopped) watcher.Stopped(revokeOnWatcherStopped);
    if (revokeOnWatcherEnumerationCompleted) watcher.EnumerationCompleted(revokeOnWatcherEnumerationCompleted);
    if (revokeOnWatcherDeviceRemoved) watcher.Removed(revokeOnWatcherDeviceRemoved);
    if (revokeOnWatcherDeviceUpdated) watcher.Updated(revokeOnWatcherDeviceUpdated);
    if (revokeOnWatcherDeviceAdded) watcher.Added(revokeOnWatcherDeviceAdded);

}
