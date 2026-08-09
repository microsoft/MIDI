// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#ifndef STRICT
#define STRICT
#endif

#include <windows.h>

#include <cguid.h>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <format>
#include <conio.h>
#include <stdio.h>

#include <devioctl.h>
#include <ks.h>
#include <ksmedia.h>

#include <wrl\module.h>
#include <wrl\event.h>
// Must precede the other wil headers: without it WIL cannot identify winrt::hresult_error
// and fail fasts instead of logging, which kills the test process instead of failing a test.
#include <wil\cppwinrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\result_macros.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Data.Json.h>

#include <filesystem>

#include <Devpkey.h>

#include "MidiDefs.h"
#include "MidiKsDef.h"
#include "WindowsMidiServices.h"
#include "MidiKSCommon.h"

