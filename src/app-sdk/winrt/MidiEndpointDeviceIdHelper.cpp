// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiEndpointDeviceIdHelper.h"
#include "MidiEndpointDeviceIdHelper.g.cpp"

// all 
#define MIDISRV_UMP_ENDPOINT_SWD_PREFIX             L"\\\\?\\swd#midisrv#midiu_"
#define MIDISRV_UMP_ENDPOINT_SWD_INTERFACE_SUFFIX   L"#{e7cce071-3c03-423f-88d3-f1045d02552b}"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    winrt::hstring MidiEndpointDeviceIdHelper::GetShortIdFromFullId(winrt::hstring const& fullEndpointDeviceId) noexcept
    {
        // we use the std::wstring version for the substr and find functions which winrt::hstring lacks
        auto cleanId = internal::NormalizeDeviceInstanceIdWStringCopy(fullEndpointDeviceId.c_str());

        if (cleanId.starts_with(MIDISRV_UMP_ENDPOINT_SWD_PREFIX))
        {
            cleanId = cleanId.substr(wcslen(MIDISRV_UMP_ENDPOINT_SWD_PREFIX));
        }
        else
        {
            // not our id
            return fullEndpointDeviceId;
        }

        if (cleanId.ends_with(MIDISRV_UMP_ENDPOINT_SWD_INTERFACE_SUFFIX))
        {
            cleanId = cleanId.substr(0, cleanId.find(MIDISRV_UMP_ENDPOINT_SWD_INTERFACE_SUFFIX));
        }
        else
        {
            // not our id
            return fullEndpointDeviceId;
        }

        return cleanId.c_str();     // will auto-convert to winrt::hstring
    }

    _Use_decl_annotations_
    winrt::hstring MidiEndpointDeviceIdHelper::GetFullIdFromShortId(winrt::hstring const& shortEndpointDeviceId) noexcept
    {
        // we don't want to start looking up all the transport codes here, so we just take on faith
        // that what is supplied is a real short endpoint device id. If it isn't, the only problem
        // they'll have is any lookups on the id will return nothing.

        return MIDISRV_UMP_ENDPOINT_SWD_PREFIX + shortEndpointDeviceId + MIDISRV_UMP_ENDPOINT_SWD_INTERFACE_SUFFIX;
    }

    _Use_decl_annotations_
    bool MidiEndpointDeviceIdHelper::IsWindowsMidiServicesDeviceId(winrt::hstring const& fullEndpointDeviceId) noexcept
    {
        auto cleanId = internal::NormalizeDeviceInstanceIdWStringCopy(fullEndpointDeviceId.c_str());

        return cleanId.starts_with(MIDISRV_UMP_ENDPOINT_SWD_PREFIX) && cleanId.ends_with(MIDISRV_UMP_ENDPOINT_SWD_INTERFACE_SUFFIX);
    }


    _Use_decl_annotations_
    winrt::hstring MidiEndpointDeviceIdHelper::NormalizeFullId(winrt::hstring const& fullEndpointDeviceId) noexcept
    {
        return internal::NormalizeDeviceInstanceIdHStringCopy(fullEndpointDeviceId);
    }

}
