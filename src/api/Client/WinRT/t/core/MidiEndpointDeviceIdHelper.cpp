// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiEndpointDeviceIdHelper.h"
#include "Enumeration.MidiEndpointDeviceIdHelper.g.cpp"

// all 
#define MIDISRV_UMP_ENDPOINT_SWD_PREFIX             L"\\\\?\\swd#midisrv#midiu_"
#define MIDISRV_UMP_ENDPOINT_SWD_INTERFACE_SUFFIX   L"#{e7cce071-3c03-423f-88d3-f1045d02552b}"

namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    _Use_decl_annotations_
    winrt::hstring MidiEndpointDeviceIdHelper::GetShortIdFromFullId(winrt::hstring const& fullEndpointDeviceId) noexcept
    {
        try
        {
        // we use the std::wstring version for the substr and find functions which winrt::hstring lacks
        auto cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(fullEndpointDeviceId.c_str());

        if (cleanId.starts_with(MIDISRV_UMP_ENDPOINT_SWD_PREFIX))
        {
            cleanId = cleanId.substr(wcslen(MIDISRV_UMP_ENDPOINT_SWD_PREFIX));
        }
        else
        {
            // not our id
            return L"";
        }

        if (cleanId.ends_with(MIDISRV_UMP_ENDPOINT_SWD_INTERFACE_SUFFIX))
        {
            cleanId = cleanId.substr(0, cleanId.find(MIDISRV_UMP_ENDPOINT_SWD_INTERFACE_SUFFIX));
        }
        else
        {
            // not our id
            return L"";
        }

        return cleanId.c_str();     // will auto-convert to winrt::hstring
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error getting short id from full id.");
            return L"";
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception getting short id from full id.");
            return L"";
        }
    }

    _Use_decl_annotations_
    winrt::hstring MidiEndpointDeviceIdHelper::GetFullIdFromShortId(winrt::hstring const& shortEndpointDeviceId) noexcept
    {
        // we don't want to start looking up all the transport codes here, so we just take on faith
        // that what is supplied is a real short endpoint device id. If it isn't, the only problem
        // they'll have is any lookups on the id will return nothing.

        try
        {
            return internal::NormalizeEndpointInterfaceIdHStringCopy(MIDISRV_UMP_ENDPOINT_SWD_PREFIX + shortEndpointDeviceId + MIDISRV_UMP_ENDPOINT_SWD_INTERFACE_SUFFIX);
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error getting full id from short id.");
            return L"";
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception getting full id from short id.");
            return L"";
        }
    }

    _Use_decl_annotations_
    bool MidiEndpointDeviceIdHelper::IsPossibleWindowsMidiServicesEndpointDeviceId(winrt::hstring const& fullEndpointDeviceId) noexcept
    {
        try
        {
        auto cleanId = internal::NormalizeEndpointInterfaceIdHStringCopy(fullEndpointDeviceId.c_str());

        return cleanId.starts_with(MIDISRV_UMP_ENDPOINT_SWD_PREFIX) && cleanId.ends_with(MIDISRV_UMP_ENDPOINT_SWD_INTERFACE_SUFFIX);
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error checking possible Windows MIDI Services endpoint device id.");
            return false;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception checking possible Windows MIDI Services endpoint device id.");
            return false;
        }
    }


    _Use_decl_annotations_
    winrt::hstring MidiEndpointDeviceIdHelper::NormalizeFullId(winrt::hstring const& fullEndpointDeviceId) noexcept
    {
        try
        {
            return internal::NormalizeEndpointInterfaceIdHStringCopy(fullEndpointDeviceId);
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error normalizing full id.");
            return L"";
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception normalizing full id.");
            return L"";
        }
    }

}
