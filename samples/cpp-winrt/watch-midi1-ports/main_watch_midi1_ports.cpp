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
#include <winrt/Windows.Devices.Midi2.Enumeration.Legacy.h>
#include <winrt/Windows.Devices.Midi2.Diagnostics.h>

using namespace winrt::Windows::Devices::Midi2;                         // SDK Core
using namespace winrt::Windows::Devices::Midi2::Enumeration;            // Enumeration
using namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy;    // MIDI 1 Enumeration
using namespace winrt::Windows::Devices::Midi2::Diagnostics;            // For diagnostics loopback endpoints


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
        std::cout << "Could not demand-start the MIDI service" << std::endl;
        return 1;
    }


    bool includeDiagnosticsEndpoints = true;

    std::cout << "Enumerating endpoints..." << std::endl;


    // creates a watcher for both input/source and output/destination ports.
    // Use CreateForFlow() if you want only one or the other
    auto watcher = MidiLegacyPortDeviceWatcher::Create();


    // if the watcher's Stop method is called
    auto OnWatcherStopped = [&](MidiLegacyPortDeviceWatcher const& /*sender*/, foundation::IInspectable const& /*args*/)
        {
            std::wcout << std::endl;
            std::wcout << L"Watcher stopped." << std::endl;
        };

    // called when initial enumeration has completed. Endpoints can appear or disappear at any time afterwards. For example,
    // a user can plug in a USB device after enumeration has completed.
    auto OnWatcherEnumerationCompleted = [&](MidiLegacyPortDeviceWatcher const& /*sender*/, foundation::IInspectable const& /*args*/)
        {
            std::wcout << std::endl;
            std::wcout << L"Initial enumeration completed." << std::endl;
        };


    // Endpoints can be removed at any time, including after enumeration has completed. This event will fire when they are removed
    auto OnWatcherDeviceRemoved = [&](MidiLegacyPortDeviceWatcher const& /*sender*/, MidiLegacyPortDeviceInformationRemovedEventArgs const& args)
        {
            std::wcout << std::endl;
            std::wcout << L"Removed: " << args.RemovedDevice().PortDeviceId().c_str() << std::endl;
        };

    // Endpoints can be updated at any time, typically due to protocol negotiation, discovery, or changes the user has made in the settings app
    auto OnWatcherDeviceUpdated = [&](MidiLegacyPortDeviceWatcher const& /*sender*/, MidiLegacyPortDeviceInformationUpdatedEventArgs const& args)
        {
            std::wcout << std::endl;
            std::wcout << "Updated: " << args.UpdatedDevice().PortDeviceId().c_str() << std::endl;

            // Show how to use the various data update flags here

            if (args.IsNameUpdated()) std::wcout << L"- Name: " << args.UpdatedDevice().Name().c_str() << std::endl;
            if (args.IsNumberUpdated()) std::wcout << L"- WinMM Port Number" << args.UpdatedDevice().Number() << std::endl;

        };

    // During initial enumeration, this event fires for each endpoint found. After initial enumeration, this will fire
    // for any new endpoints added (examples: USB Device plugged in, Virtual Device created by an app, etc.
    auto OnWatcherDeviceAdded = [&](MidiLegacyPortDeviceWatcher const& /*sender*/, MidiLegacyPortDeviceInformationAddedEventArgs const& args)
        {
            std::wcout << std::endl;
            std::wcout << L"Added  :      " << args.AddedDevice().Name().c_str() << std::endl;
            std::wcout << L"              " << args.AddedDevice().PortDeviceId().c_str() << std::endl ;
            std::wcout << L"WinMM Number: " << args.AddedDevice().Number() << std::endl;
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

}
