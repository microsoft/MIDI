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

    //// Check to see if Windows MIDI Services is installed and running on this PC
    //if (!MidiServicesInitializer::EnsureServiceAvailable())
    //{
    //    // you may wish to fallback to an older MIDI API if it suits your application's workflow
    //    std::cout << std::endl << "** Windows MIDI Services is not running on this PC **" << std::endl;

    //    return 1;
    //}
    //else
    //{
    //    std::cout << std::endl << "Verified that the MIDI Service is available and started" << std::endl;

    //    // bootstrap the SDK runtime
    //    MidiServicesInitializer::InitializeDesktopAppSdkRuntime();
    //}


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



    auto OnWatcherStopped = [&](MidiEndpointDeviceWatcher const& /*sender*/, foundation::IInspectable const& /*args*/)
        {
            std::cout << std::endl;
            std::cout << "Watcher stopped." << std::endl;
        };

    auto OnWatcherEnumerationCompleted = [&](MidiEndpointDeviceWatcher const& /*sender*/, foundation::IInspectable const& /*args*/)
        {
            std::cout << std::endl;
            std::cout << "Initial enumeration completed." << std::endl;
        };



    auto OnWatcherDeviceRemoved = [&](MidiEndpointDeviceWatcher const& /*sender*/, MidiEndpointDeviceInformationRemovedEventArgs const& args)
        {
            std::cout << std::endl;
            std::cout << "Removed: " << winrt::to_string(args.EndpointDeviceId()) << std::endl;
        };

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

    auto OnWatcherDeviceAdded = [&](MidiEndpointDeviceWatcher const& /*sender*/, MidiEndpointDeviceInformationAddedEventArgs const& args)
        {
            std::cout << std::endl;
            std::cout << "Added  : " << winrt::to_string(args.AddedDevice().Name()) << std::endl;
            std::cout << "         " << winrt::to_string(args.AddedDevice().EndpointDeviceId()) << std::endl ;
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
