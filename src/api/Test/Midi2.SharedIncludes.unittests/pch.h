// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include <windows.h>
#include <cguid.h>
#include <ks.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <ppltasks.h>
#include <WexTestClass.h>

#include <winrt/Windows.Foundation.h>


// we need cpp 20 for std::format, which is used in the midi_naming include file
#include <string>
#include <format>

#include <iostream>

#include <wstring_util.h>

#include "WindowsMidiServices.h"
#include "WindowsMidiServices_i.c"


namespace internal = ::WindowsMidiServicesInternal;


#include <midi_naming.h>

#include "NamingTests.h"

#endif //PCH_H
