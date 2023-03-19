#pragma once

#include <unknwn.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Storage.Streams.h>

#include <winrt/Windows.Networking.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Networking.ServiceDiscovery.Dnssd.h>

#include <winrt/Windows.Storage.Streams.h>


#include <cstdint>
#include <vector>
#include <string>

namespace json = winrt::Windows::Data::Json;
namespace streams = winrt::Windows::Storage::Streams;
namespace networking = winrt::Windows::Networking;
namespace sock = winrt::Windows::Networking::Sockets;
namespace dnssd = winrt::Windows::Networking::ServiceDiscovery::Dnssd;
