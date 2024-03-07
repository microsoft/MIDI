// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code

#include "pch.h"
#include <iostream>

using namespace winrt::Windows::Devices::Midi2;        // API


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
    winrt::init_apartment();

    bool includeDiagnosticsEndpoints = true;

    std::cout << "Enumerating endpoints..." << std::endl;

    auto filter = MidiEndpointDeviceInformationFilter::IncludeClientByteStreamNative |
        MidiEndpointDeviceInformationFilter::IncludeClientUmpNative |
        MidiEndpointDeviceInformationFilter::IncludeVirtualDeviceResponder;

    if (includeDiagnosticsEndpoints)
    {
        filter |= MidiEndpointDeviceInformationFilter::IncludeDiagnosticLoopback;
    }

    auto watcher = MidiEndpointDeviceWatcher::CreateWatcher(filter);



    auto OnWatcherStopped = [&](MidiEndpointDeviceWatcher const& /*sender*/, foundation::IInspectable const& /*args*/)
        {
            std::cout << std::endl;
            std::cout << "Watcher stopped." << std::endl;
        };

    auto OnWatcherEnumerationCompleted = [&](MidiEndpointDeviceWatcher const& /*sender*/, foundation::IInspectable const& args)
        {
            std::cout << std::endl;
            std::cout << "Initial enumeration completed." << std::endl;
        };

    auto OnWatcherDeviceRemoved = [&](MidiEndpointDeviceWatcher const& /*sender*/, winrt::Windows::Devices::Enumeration::DeviceInformationUpdate const& args)
        {
            std::cout << std::endl;
            std::cout << "Removed: " << winrt::to_string(args.Id()) << std::endl;
        };

    auto OnWatcherDeviceUpdated = [&](MidiEndpointDeviceWatcher const& /*sender*/, MidiEndpointDeviceInformationUpdateEventArgs const& args)
        {
            std::cout << std::endl;
            std::cout << "Updated: " << winrt::to_string(args.Id()) << std::endl;

            // TODO: Show how to use the various data update flags here
        };

    auto OnWatcherDeviceAdded = [&](MidiEndpointDeviceWatcher const& /*sender*/, MidiEndpointDeviceInformation const& args)
        {
            std::cout << std::endl;
            std::cout << "Added  : " << winrt::to_string(args.Name()) << std::endl;
            std::cout << "         " << winrt::to_string(args.Id()) << std::endl ;
        };


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

    watcher.Stopped(revokeOnWatcherStopped);
    watcher.EnumerationCompleted(revokeOnWatcherEnumerationCompleted);
    watcher.Removed(revokeOnWatcherDeviceRemoved);
    watcher.Updated(revokeOnWatcherDeviceUpdated);
    watcher.Added(revokeOnWatcherDeviceAdded);

}
