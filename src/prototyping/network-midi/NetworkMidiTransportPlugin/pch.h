#pragma once

#include <unknwn.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Storage.Streams.h>

#include <winrt/Windows.Networking.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Networking.ServiceDiscovery.Dnssd.h>

// had to add the other project's generated files folder to the include directories.
// shouldn't have needed to do that here.
//#include <Windows.Devices.Midi.ServiceContracts.h>


#include <cstdint>
#include <vector>
#include <string>

namespace json = winrt::Windows::Data::Json;
namespace streams = winrt::Windows::Storage::Streams;
namespace networking = winrt::Windows::Networking;
namespace sock = winrt::Windows::Networking::Sockets;
namespace dnssd = winrt::Windows::Networking::ServiceDiscovery::Dnssd;
namespace collections = winrt::Windows::Foundation::Collections;


// Did this because I had to merge the projects for stupid prototype reasons
//namespace midiservicecontracts = winrt::Windows::Devices::Midi::ServiceContracts;
//namespace midiservicecontracts = winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin;
