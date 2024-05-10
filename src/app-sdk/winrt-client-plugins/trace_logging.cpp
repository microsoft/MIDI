// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include <pch.h>

#include <winmeta.h>

namespace WindowsMidiServicesInternal
{
    bool g_traceLoggingRegistered = false;

    // Microsoft.Windows.Devices.Midi2.Api hashed tracing guid: 2c7d1437-4f43-5713-170b-adbe4cff4dff
    // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Devices.Midi2.Api").Guid

    TRACELOGGING_DEFINE_PROVIDER(
        g_hLoggingProvider,
        TRACELOGGING_PROVIDER_NAME,                         
        // {2c7d1437-4f43-5713-170b-adbe4cff4dff}
        (0x2c7d1437, 0x4f43, 0x5713, 0x17, 0x0b, 0xad, 0xbe, 0x4c, 0xff, 0x4d, 0xff));


    void WINAPI LoggingProviderEnabledCallback(
        _In_      LPCGUID /*sourceId*/,
        _In_      ULONG /*isEnabled*/,
        _In_      UCHAR /*level*/,
        _In_      ULONGLONG /*matchAnyKeyword*/,
        _In_      ULONGLONG /*matchAllKeywords*/,
        _In_opt_  PEVENT_FILTER_DESCRIPTOR /*filterData*/,
        _In_opt_  PVOID /*callbackContext*/)
    {
        OutputDebugString(L"" __FUNCTION__);

        //g_IsLoggingProviderEnabled = !!isEnabled;
        //g_LoggingProviderLevel = level;
        //g_LoggingProviderMatchAnyKeyword = matchAnyKeyword;
    }

    void RegisterTraceLogging()
    {
        // HRESULT hr = S_OK;
        TraceLoggingRegisterEx(g_hLoggingProvider, LoggingProviderEnabledCallback, nullptr);

        ////Generate the ActivityId used to track the session
        //hr = CoCreateGuid(&g_LoggingProviderActivityId);

        //if (FAILED(hr))
        //{
        //    TraceLoggingWriteActivity(
        //        g_hLoggingProvider,
        //        "CreateGuidError",
        //        nullptr,
        //        nullptr,
        //        TraceLoggingHResult(hr));

        //    g_LoggingProviderActivityId = GUID_NULL;
        //};

        g_traceLoggingRegistered = true;
    }

    void UnregisterTraceLogging()
    {
        TraceLoggingUnregister(g_hLoggingProvider);

        g_traceLoggingRegistered = true;
    }



    // may want to allow passing in a keyword so we can be finer-grained
    _Use_decl_annotations_
    void LogHresultError(
        const char* location, 
        const wchar_t* message, 
        winrt::hresult_error const& ex
    ) noexcept
    {
        //OutputDebugString(L"" __FUNCTION__ L"API HRESULT Error. Use tracing provider for details.");

        if (!g_traceLoggingRegistered) RegisterTraceLogging();

        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.HresultError",
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API_GENERAL),
            TraceLoggingString(location, MIDI_TRACE_LOGGING_LOCATION_FIELD),
            TraceLoggingWideString(message, MIDI_TRACE_LOGGING_MESSAGE_FIELD),
            TraceLoggingHResult(ex.code(), MIDI_TRACE_LOGGING_HRESULT_FIELD),
            TraceLoggingWideString(ex.message().c_str(), MIDI_TRACE_LOGGING_ERROR_FIELD)
        );
    }

    _Use_decl_annotations_
    void LogHresultError(
        const char* location,
        const wchar_t* message,
        winrt::hresult_error const& ex,
        winrt::hstring endpointDeviceId
    ) noexcept
    {
        //OutputDebugString(L"" __FUNCTION__ L"API HRESULT Error. Use tracing provider for details.");

        if (!g_traceLoggingRegistered) RegisterTraceLogging();

        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.HresultError",
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API_GENERAL),
            TraceLoggingString(location, MIDI_TRACE_LOGGING_LOCATION_FIELD),
            TraceLoggingWideString(message, MIDI_TRACE_LOGGING_MESSAGE_FIELD),
            TraceLoggingHResult(ex.code(), MIDI_TRACE_LOGGING_HRESULT_FIELD),
            TraceLoggingWideString(ex.message().c_str(), MIDI_TRACE_LOGGING_ERROR_FIELD),
            TraceLoggingWideString(endpointDeviceId.c_str(), MIDI_TRACE_LOGGING_ENDPOINT_DEVICE_ID_FIELD)
        );
    }


    _Use_decl_annotations_
    void LogHresultError(
        const char* location,
        const wchar_t* message,
        HRESULT hr
    ) noexcept
    {
        //OutputDebugString(L"" __FUNCTION__ L"API HRESULT Error. Use tracing provider for details.");

        if (!g_traceLoggingRegistered) RegisterTraceLogging();

        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.HresultError",
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API_GENERAL),
            TraceLoggingString(location, MIDI_TRACE_LOGGING_LOCATION_FIELD),
            TraceLoggingWideString(message, MIDI_TRACE_LOGGING_MESSAGE_FIELD),
            TraceLoggingHResult(hr, MIDI_TRACE_LOGGING_HRESULT_FIELD)
        );
    }

    _Use_decl_annotations_
    void LogHresultError(
        const char* location,
        const wchar_t* message,
        HRESULT hr,
        winrt::hstring endpointDeviceId
    ) noexcept
    {
        //OutputDebugString(L"" __FUNCTION__ L"API HRESULT Error. Use tracing provider for details.");

        if (!g_traceLoggingRegistered) RegisterTraceLogging();

        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.HresultError",
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API_GENERAL),
            TraceLoggingString(location, MIDI_TRACE_LOGGING_LOCATION_FIELD),
            TraceLoggingWideString(message, MIDI_TRACE_LOGGING_MESSAGE_FIELD),
            TraceLoggingHResult(hr, MIDI_TRACE_LOGGING_HRESULT_FIELD),
            TraceLoggingWideString(endpointDeviceId.c_str(), MIDI_TRACE_LOGGING_ENDPOINT_DEVICE_ID_FIELD)
        );
    }


    _Use_decl_annotations_
    void LogStandardExceptionError(
        const char* location,
        const wchar_t* message,
        std::exception const& ex
    ) noexcept
    {
        if (!g_traceLoggingRegistered) RegisterTraceLogging();

        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.StandardExceptionError",
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API_GENERAL),
            TraceLoggingString(location, MIDI_TRACE_LOGGING_LOCATION_FIELD),
            TraceLoggingWideString(message, MIDI_TRACE_LOGGING_MESSAGE_FIELD),
            TraceLoggingString(ex.what(), MIDI_TRACE_LOGGING_ERROR_FIELD)
        );
    }

    _Use_decl_annotations_
    void LogStandardExceptionError(
        const char* location,
        const wchar_t* message,
        std::exception const& ex,
        winrt::hstring endpointDeviceId
    ) noexcept
    {
        if (!g_traceLoggingRegistered) RegisterTraceLogging();

        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.StandardExceptionError",
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API_GENERAL),
            TraceLoggingString(location, MIDI_TRACE_LOGGING_LOCATION_FIELD),
            TraceLoggingWideString(message, MIDI_TRACE_LOGGING_MESSAGE_FIELD),
            TraceLoggingString(ex.what(), MIDI_TRACE_LOGGING_ERROR_FIELD),
            TraceLoggingWideString(endpointDeviceId.c_str(), MIDI_TRACE_LOGGING_ENDPOINT_DEVICE_ID_FIELD)
        );
    }


    _Use_decl_annotations_
    void LogGeneralError(
        const char* location, 
        const wchar_t* message
    ) noexcept
    {
        //OutputDebugString(L"" __FUNCTION__ L"API General Error. Use tracing provider for details.");

        if (!g_traceLoggingRegistered) RegisterTraceLogging();

        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.GeneralError",
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API_GENERAL),
            TraceLoggingString(location, MIDI_TRACE_LOGGING_LOCATION_FIELD),
            TraceLoggingWideString(message, MIDI_TRACE_LOGGING_MESSAGE_FIELD)
            );
    }

    _Use_decl_annotations_
    void LogGeneralError(
        const char* location,
        const wchar_t* message,
        winrt::hstring endpointDeviceId
    ) noexcept
    {
        //OutputDebugString(L"" __FUNCTION__ L"API General Error. Use tracing provider for details.");

        if (!g_traceLoggingRegistered) RegisterTraceLogging();

        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.GeneralError",
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API_GENERAL),
            TraceLoggingString(location, MIDI_TRACE_LOGGING_LOCATION_FIELD),
            TraceLoggingWideString(message, MIDI_TRACE_LOGGING_MESSAGE_FIELD),
            TraceLoggingWideString(endpointDeviceId.c_str(), MIDI_TRACE_LOGGING_ENDPOINT_DEVICE_ID_FIELD)
        );
    }


    _Use_decl_annotations_
    void LogInfo(
        const char* location,
        const wchar_t* message,
        winrt::hstring endpointDeviceId
    ) noexcept

    {
        if (!g_traceLoggingRegistered) RegisterTraceLogging();

        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.Info",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingKeyword(TRACE_KEYWORD_API_GENERAL),
            TraceLoggingString(location, MIDI_TRACE_LOGGING_LOCATION_FIELD),
            TraceLoggingWideString(message, MIDI_TRACE_LOGGING_MESSAGE_FIELD),
            TraceLoggingWideString(endpointDeviceId.c_str(), MIDI_TRACE_LOGGING_ENDPOINT_DEVICE_ID_FIELD)
        );
    }

    _Use_decl_annotations_
    void LogInfo(
        const char* location,
        const wchar_t* message
    ) noexcept
    {
        if (!g_traceLoggingRegistered) RegisterTraceLogging();

        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.Info",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingKeyword(TRACE_KEYWORD_API_GENERAL),
            TraceLoggingString(location, MIDI_TRACE_LOGGING_LOCATION_FIELD),
            TraceLoggingWideString(message, MIDI_TRACE_LOGGING_MESSAGE_FIELD)
            );
    }

    _Use_decl_annotations_
    void LogUmpDataValidationError(
        const char* location, 
        const wchar_t* message, 
        const uint32_t firstWord, 
        const uint64_t timestamp
    ) noexcept
    {
        //OutputDebugString(L"" __FUNCTION__ L"API UMP Validation Error. Use tracing provider for details.");

        if (!g_traceLoggingRegistered) RegisterTraceLogging();

        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.UmpDataValidationError",
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API_DATA_VALIDATION),
            TraceLoggingString(location, MIDI_TRACE_LOGGING_LOCATION_FIELD),
            TraceLoggingWideString(message, MIDI_TRACE_LOGGING_MESSAGE_FIELD),
            TraceLoggingHexUInt32(firstWord, MIDI_TRACE_LOGGING_MESSAGE_WORD0_FIELD),
            TraceLoggingUInt64(timestamp, MIDI_TRACE_LOGGING_MESSAGE_TIMESTAMP_FIELD)
        );
    }

    _Use_decl_annotations_
    void LogUmpSizeValidationError(
        const char* location, 
        const wchar_t* message, 
        const uint32_t providedSizeInWords, 
        const uint64_t timestamp
    ) noexcept
    {
        //OutputDebugString(L"" __FUNCTION__ L"API UMP Size Validation Error. Use tracing provider for details.");

        if (!g_traceLoggingRegistered) RegisterTraceLogging();

        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.UmpSizeValidationError",
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API_DATA_VALIDATION),
            TraceLoggingString(location, MIDI_TRACE_LOGGING_LOCATION_FIELD),
            TraceLoggingWideString(message, MIDI_TRACE_LOGGING_MESSAGE_FIELD),
            TraceLoggingHexUInt32(providedSizeInWords, "ProvidedSizeInWords"),
            TraceLoggingUInt64(timestamp, MIDI_TRACE_LOGGING_MESSAGE_TIMESTAMP_FIELD)
        );
    }


}