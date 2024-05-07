// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// tracelogging information:
// https://learn.microsoft.com/en-us/windows/win32/api/traceloggingprovider/nf-traceloggingprovider-tracelogging_define_provider

#pragma once


//#include "WinEventLogLevels.h"
#include <TraceLoggingProvider.h>

// qualified with API so as not to intefere with service or SDK tracing
#define TRACELOGGING_PROVIDER_NAME          "Microsoft.Devices.Midi2.Api"
#define TRACE_KEYWORD_API_GENERAL           0x0000000000000001
#define TRACE_KEYWORD_API_DATA_VALIDATION   0x0000000000000002

#define MIDI_TRACE_LOGGING_LOCATION_FIELD               "Location"
#define MIDI_TRACE_LOGGING_MESSAGE_FIELD                "Message"
#define MIDI_TRACE_LOGGING_ENDPOINT_DEVICE_ID_FIELD     "Endpoint Id"
#define MIDI_TRACE_LOGGING_HRESULT_FIELD                "HRESULT"
#define MIDI_TRACE_LOGGING_ERROR_FIELD                  "Error"
#define MIDI_TRACE_LOGGING_MESSAGE_TIMESTAMP_FIELD      "MIDI Timestamp"
#define MIDI_TRACE_LOGGING_MESSAGE_WORD0_FIELD          "MIDI Word0"



using namespace std;

namespace WindowsMidiServicesInternal
{
    TRACELOGGING_DECLARE_PROVIDER(g_hLoggingProvider);

    extern GUID g_LoggingProviderActivityId;

 //   extern bool g_traceLoggingRegistered;

    void RegisterTraceLogging();
    void UnregisterTraceLogging();


    void LogInfo(
        _In_z_ const char* location,
        _In_z_ const wchar_t* message) noexcept;

    void LogInfo(
        _In_z_ const char* location,
        _In_z_ const wchar_t* message,
        _In_ winrt::hstring endpointDeviceId) noexcept;



    void LogHresultError(
        _In_z_ const char* location,
        _In_z_ const wchar_t* message,
        _In_ winrt::hresult_error const& ex) noexcept;

    void LogHresultError(
        _In_z_ const char* location,
        _In_z_ const wchar_t* message,
        _In_ winrt::hresult_error const& ex,
        _In_ winrt::hstring endpointDeviceId) noexcept;

    void LogHresultError(
        _In_z_ const char* location,
        _In_z_ const wchar_t* message,
        _In_ HRESULT const hr) noexcept;

    void LogHresultError(
        _In_z_ const char* location,
        _In_z_ const wchar_t* message,
        _In_ HRESULT const hr,
        _In_ winrt::hstring endpointDeviceId) noexcept;


    void LogStandardExceptionError(
        _In_z_ const char* location,
        _In_z_ const wchar_t* message,
        _In_ std::exception const& ex) noexcept;

    void LogStandardExceptionError(
        _In_z_ const char* location,
        _In_z_ const wchar_t* message,
        _In_ std::exception const& ex,
        _In_ winrt::hstring endpointDeviceId) noexcept;

    void LogGeneralError(
        _In_z_ const char* location,
        _In_z_ const wchar_t* message) noexcept;

    void LogGeneralError(
        _In_z_ const char* location,
        _In_z_ const wchar_t* message,
        _In_ winrt::hstring endpointDeviceId) noexcept;

    void LogUmpDataValidationError(
        _In_z_ const char* location,
        _In_z_ const wchar_t* message,
        _In_ const uint32_t firstWord,
        _In_ const uint64_t timestamp) noexcept;

    void LogUmpSizeValidationError(
        _In_z_ const char* location,
        _In_z_ const wchar_t* message,
        _In_ const uint32_t providedSizeInWords,
        _In_ const uint64_t timestamp) noexcept;


}