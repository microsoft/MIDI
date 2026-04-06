// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiImageAssetHelper.h"
#include "Utilities.Metadata.MidiImageAssetHelper.g.cpp"

#include <filesystem>

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::Metadata::implementation
{
#define BASE_TRANSPORT_IMAGE_PATH                   L"%allusersprofile%\\Microsoft\\MIDI\\Assets\\Transports\\"
#define BASE_ENDPOINT_IMAGE_PATH                    L"%allusersprofile%\\Microsoft\\MIDI\\Assets\\Endpoints\\"
#define SMALL_IMAGE_DEFAULT_SUFFIX_AND_EXTENSION    L"-small.svg"
#define ENDPOINT_SMALL_IMAGE_PREFIX_DEFAULT         L"default-"
#define ENDPOINT_SMALL_IMAGE_DEFAULT                L"default-small.svg"


    inline winrt::hstring ExpandPath(_In_ winrt::hstring sourcePath)
    {
        wchar_t expanded[MAX_PATH + 1]{ 0 };

        /*auto numChars = */ ExpandEnvironmentStrings(sourcePath.c_str(), (LPWSTR)&expanded, MAX_PATH);

        return winrt::hstring(expanded);
    }

    bool FileNameValid(winrt::hstring fileName)
    {
        if (fileName.empty())
        {
            return false;
        }

        // the APIs we call to validate the file only support MAX_PATH
        if (fileName.size() > MAX_PATH)
        {
            return false;
        }

        // no URLs allowed
        if (PathIsURL(fileName.c_str()))
        {
            return false;
        }

        // No wildcards or invalid path characters
        for (auto const& ch : fileName)
        {
            auto result = PathGetCharType(ch);

            if (result == GCT_INVALID  || result == GCT_WILD)
            {
                return false;
            }

            // we don't allow path expansion in the stored path 
            if (ch == '%')
            {
                return false;
            }
        }

        return true;
    }

    bool FileNameContainsPath(winrt::hstring fileName)
    {
        // TODO: this should use regular Windows APIs

        for (auto const & ch : fileName)
        {
            if (ch =='/' || ch == '\\' || ch == ':')
            {
                return true;
            }
        }

        return false;
    }

    bool FileExists(winrt::hstring fullPath)
    {
        return PathFileExists(fullPath.c_str());
    }

    _Use_decl_annotations_
    winrt::hstring MidiImageAssetHelper::GetDefaultImageFullPathForTransport(
        midi2::Reporting::MidiServiceTransportPluginInfo const& transportPluginInfo) noexcept
    {
        return ExpandPath(BASE_TRANSPORT_IMAGE_PATH) + transportPluginInfo.TransportCode + SMALL_IMAGE_DEFAULT_SUFFIX_AND_EXTENSION;
    }

    _Use_decl_annotations_
    winrt::hstring MidiImageAssetHelper::GetImageFullPathForTransport(
        midi2::Reporting::MidiServiceTransportPluginInfo const& transportPluginInfo) noexcept
    {
        auto fileName = transportPluginInfo.ImageFileName;

        if (!FileNameValid(fileName))
        {
            // Return the default file for the transport
            return GetDefaultImageFullPathForTransport(transportPluginInfo);
        }

        if (FileNameContainsPath(fileName) && FileExists(fileName))
        {
            return fileName;
        }

        // otherwise, it's a file in our standard folder. Check to see 

        return ExpandPath(BASE_TRANSPORT_IMAGE_PATH) + fileName;
    }


    _Use_decl_annotations_
    winrt::hstring MidiImageAssetHelper::GetDefaultImageFullPathForEndpoint(
        midi2::MidiEndpointDeviceInformation const& endpointDeviceInformation) noexcept
    {
        if (endpointDeviceInformation == nullptr)
        {
            LOG_IF_FAILED(E_INVALIDARG);
            return L"";
        }

        auto code = endpointDeviceInformation.GetTransportSuppliedInfo().TransportCode;
        auto fileName = winrt::hstring{ ENDPOINT_SMALL_IMAGE_PREFIX_DEFAULT } + code + winrt::hstring{ SMALL_IMAGE_DEFAULT_SUFFIX_AND_EXTENSION };

        fileName = ExpandPath(BASE_ENDPOINT_IMAGE_PATH) + fileName;

        if (FileExists(fileName))
        {
            return fileName;
        }
        else
        {
            // if all else fails, we go with the straight-up plain default
            return ExpandPath(BASE_ENDPOINT_IMAGE_PATH) + winrt::hstring{ ENDPOINT_SMALL_IMAGE_DEFAULT };
        }

    }



    _Use_decl_annotations_
    winrt::hstring MidiImageAssetHelper::GetImageFullPathForEndpoint(
        midi2::MidiEndpointDeviceInformation const& endpointDeviceInformation) noexcept
    {
        if (endpointDeviceInformation == nullptr)
        {
            LOG_IF_FAILED(E_INVALIDARG);
            return L"";
        }

        if (EndpointHasValidCustomImageAsset(endpointDeviceInformation))
        {
            return GetFullCustomImageAssetPathForEndpointImage(endpointDeviceInformation);
        }
        else
        {
            return GetDefaultImageFullPathForEndpoint(endpointDeviceInformation);
        }
    }



    _Use_decl_annotations_
    bool MidiImageAssetHelper::EndpointHasValidCustomImageAsset(
        midi2::MidiEndpointDeviceInformation const& endpointDeviceInformation) noexcept
    {
        if (endpointDeviceInformation == nullptr)
        {
            LOG_IF_FAILED(E_INVALIDARG);
            return false;
        }

        try
        {
            auto fileNamePropertyValue = endpointDeviceInformation.GetUserSuppliedInfo().ImageFileName;

            if (fileNamePropertyValue.empty())
            {
                return false;
            }

            std::filesystem::path p(fileNamePropertyValue.c_str());

            if (p.filename().empty())
            {
                return false;
            }

            auto fullPath = ExpandPath(BASE_ENDPOINT_IMAGE_PATH) + p.filename();

            return FileExists(fullPath);
        }
        catch (winrt::hresult_error ex)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to check if endpoint has valid image asset", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );
        }
        catch (...)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"General exception. Unable to check if endpoint has valid image asset", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }

        return false;
    }

    _Use_decl_annotations_
    winrt::hstring MidiImageAssetHelper::GetFullCustomImageAssetPathForEndpointImage(
        midi2::MidiEndpointDeviceInformation const& endpointDeviceInformation) noexcept
    {
        if (endpointDeviceInformation == nullptr)
        {
            LOG_IF_FAILED(E_INVALIDARG);
            return L"";
        }

        try
        {
            auto fileNamePropertyValue = endpointDeviceInformation.GetUserSuppliedInfo().ImageFileName;

            if (fileNamePropertyValue.empty())
            {
                return L"";
            }

            std::filesystem::path p(fileNamePropertyValue.c_str());

            if (p.filename().empty())
            {
                return L"";
            }

            return ExpandPath(BASE_ENDPOINT_IMAGE_PATH) + p.filename();
        }
        catch (winrt::hresult_error ex)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to get full image asset path", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );
        }
        catch (...)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"General exception. Unable to get full image asset path", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }

        return L"";
    }





}
