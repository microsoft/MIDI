// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

#include <Windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.Background.h>
#include <winrt/windows.devices.bluetooth.genericattributeprofile.h>
#include <winrt/Windows.Data.Json.h>

#include <winrt/Windows.Storage.Streams.h>

#include <winerror.h>

namespace json = ::winrt::Windows::Data::Json;
namespace enumeration = ::winrt::Windows::Devices::Enumeration;
namespace foundation = ::winrt::Windows::Foundation;
namespace collections = ::winrt::Windows::Foundation::Collections;
namespace streams = ::winrt::Windows::Storage::Streams;

using namespace winrt::Windows::Devices::Bluetooth;
using namespace winrt::Windows::Devices::Bluetooth::Advertisement;
using namespace winrt::Windows::Devices::Bluetooth::Background;
using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

#include <iostream>
#include <iomanip>

#include "abstraction_defs.h"
#include "ble_utilities.h"

#include "MidiBleDevice.h"
#include "MidiBleBiDi.h"
#include "MidiBleEndpointManager.h"
#include "MidiBleAdvertisedEndpointManager.h"