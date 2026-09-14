// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <windows.h>
#include <shellapi.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <vector>

// Must come before the other WIL headers: it installs the translator that teaches WIL about
// cppwinrt exceptions. Without it, a winrt::hresult_error reaching CATCH_RETURN is treated as an
// unknown exception and fail-fasts the process instead of being returned as an HRESULT.
#include <wil/cppwinrt.h>
#include <wil/resource.h>
#include <wil/result_macros.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Transports.Network.h>

#include "..\..\Transport\UdpNetworkMidi2Transport\network_notification_defs.h"
#include "..\network-midi-setup\network_setup_protocol_defs.h"

#include "notification_settings_defs.h"
#include "resource.h"

#include "..\midi-app-shared\SingleInstance.h"

#include "AppSettings.h"
#include "ToastSender.h"
#include "RegistryChangeWatcher.h"
#include "NetworkApprovalNotifier.h"
