// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#define WINAPI_PARTITION_DESKTOP 1

#include <ntstatus.h>

#define WIN32_NO_STATUS
#include <windows.h>
#include <winternl.h>
#undef WIN32_NO_STATUS

#include <assert.h>
#include <devioctl.h>
#include <wrl\implements.h>
#include <ks.h>
#include <ksmedia.h>
#include <avrt.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <ppltasks.h>
#include <WexTestClass.h>

#ifndef LOG_OUTPUT
#define LOG_OUTPUT(fmt, ...)  WEX::Logging::Log::Comment(WEX::Common::String().Format(fmt, __VA_ARGS__))
#endif

