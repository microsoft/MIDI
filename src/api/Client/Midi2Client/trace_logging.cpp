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
        winrt::hresult_error const& ex) noexcept
    {
        //OutputDebugString(L"" __FUNCTION__ L"API HRESULT Error. Use tracing provider for details.");

        if (!g_traceLoggingRegistered) RegisterTraceLogging();

        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.HresultError",
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API_GENERAL),
            TraceLoggingHResult(ex.code(), "HRESULT"),
            TraceLoggingString(location, "Location"),
            TraceLoggingWideString(message, "Message"),
            TraceLoggingWideString(ex.message().c_str(), "Error")
        );
    }

    _Use_decl_annotations_
    void LogHresultError(
        const char* location,
        const wchar_t* message,
        HRESULT hr) noexcept
    {
        //OutputDebugString(L"" __FUNCTION__ L"API HRESULT Error. Use tracing provider for details.");

        if (!g_traceLoggingRegistered) RegisterTraceLogging();

        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.HresultError",
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API_GENERAL),
            TraceLoggingHResult(hr, "HRESULT"),
            TraceLoggingString(location, "Location"),
            TraceLoggingWideString(message, "Message")
        );
    }


    _Use_decl_annotations_
    void LogGeneralError(
        const char* location, 
        const wchar_t* message) noexcept
    {
        OutputDebugString(L"" __FUNCTION__ L"API General Error. Use tracing provider for details.");

        if (!g_traceLoggingRegistered) RegisterTraceLogging();

        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.GeneralError",
            TraceLoggingOpcode(ERROR),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API_GENERAL),
            TraceLoggingString(location, "Location"),
            TraceLoggingWideString(message, "Message")
        );
    }

    void LogInfo(
        _In_z_ const char* location,
        _In_z_ const wchar_t* message) noexcept
    {
        if (!g_traceLoggingRegistered) RegisterTraceLogging();

        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.Info",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingKeyword(TRACE_KEYWORD_API_GENERAL),
            TraceLoggingString(location, "Location"),
            TraceLoggingWideString(message, "Message")
        );
    }

    _Use_decl_annotations_
    void LogUmpDataValidationError(
        const char* location, 
        const wchar_t* message, 
        const uint32_t firstWord, 
        const uint64_t timestamp) noexcept
    {
        OutputDebugString(L"" __FUNCTION__ L"API UMP Validation Error. Use tracing provider for details.");

        if (!g_traceLoggingRegistered) RegisterTraceLogging();

        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.UmpDataValidationError",
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API_DATA_VALIDATION),
            TraceLoggingHexUInt32(firstWord, "UMPFirstWord"),
            TraceLoggingUInt64(timestamp, "MIDITimestamp"),
            TraceLoggingString(location, "Location"),
            TraceLoggingWideString(message, "Message")
        );
    }

    _Use_decl_annotations_
    void LogUmpSizeValidationError(
        const char* location, 
        const wchar_t* message, 
        const uint32_t providedSizeInWords, 
        const uint64_t timestamp) noexcept
    {
        OutputDebugString(L"" __FUNCTION__ L"API UMP Size Validation Error. Use tracing provider for details.");

        if (!g_traceLoggingRegistered) RegisterTraceLogging();

        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.UmpSizeValidationError",
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API_DATA_VALIDATION),
            TraceLoggingHexUInt32(providedSizeInWords, "ProvidedSizeInWords"),
            TraceLoggingUInt64(timestamp, "MIDITimestamp"),
            TraceLoggingString(location, "Location"),
            TraceLoggingWideString(message, "Message")
        );
    }


}