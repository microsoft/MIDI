// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. Precompiled header for Windows.Devices.Midi2.Transports.Rtp.
// ============================================================================

#pragma once

#include <windows.h>
#include <unknwn.h>
#include <restrictederrorinfo.h>
#include <hstring.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <string>
#include <vector>

// wil\cppwinrt.h must come before any other WinRT header
#include <wil\cppwinrt.h>
#include <wil\resource.h>
#include <wil\result_macros.h>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#undef GetObject
#include <winrt/Windows.Data.Json.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.ServiceConfig.h>
#include <winrt/Windows.Devices.Midi2.Reporting.h>

namespace json = ::winrt::Windows::Data::Json;
namespace foundation = ::winrt::Windows::Foundation;
namespace collections = ::winrt::Windows::Foundation::Collections;
namespace svc = ::winrt::Windows::Devices::Midi2::ServiceConfig;
namespace rpt = ::winrt::Windows::Devices::Midi2::Reporting;

namespace winrt::Windows::Devices::Midi2::Transports::Rtp {}
namespace rtp = ::winrt::Windows::Devices::Midi2::Transports::Rtp;

namespace WindowsMidiServicesInternal {}
namespace internal = ::WindowsMidiServicesInternal;

#include <json_defs.h>
#include <wstring_util.h>
#include <resource_util.h>

// the transport's own key names, error codes and defaults, so the two cannot drift apart
#include "..\transport\transport_defs.h"
#include "..\transport\rtp_json_defs.h"
#include "..\transport\rtp_transport_error_codes.h"

#include "resource.h"
#include "RtpSdkDefs.h"
#include "RtpSdkJson.h"
