// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiEndpointDeviceHelper.h"
#include "Enumeration.MidiEndpointDeviceHelper.g.cpp"

// all 
#define MIDISRV_UMP_ENDPOINT_SWD_PREFIX                         L"\\\\?\\swd#midisrv#midiu_"
#define MIDISRV_UMP_ENDPOINT_SWD_INTERFACE_SUFFIX               L"#{e7cce071-3c03-423f-88d3-f1045d02552b}"

#define MIDISRV_LEGACY_PORT_SWD_PREFIX                          L"\\\\?\\swd#mmdevapi#midiu_"
#define MIDISRV_LEGACY_PORT_DESTINATION_SWD_INTERFACE_SUFFIX    L"#{6dc23320-ab33-4ce4-80d4-bbb3ebbf2814}"
#define MIDISRV_LEGACY_PORT_SOURCE_SWD_INTERFACE_SUFFIX         L"#{504be32c-ccf6-4d2c-b73f-6f8b3747e22b}"


namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    _Use_decl_annotations_
    winrt::hstring MidiEndpointDeviceHelper::EnsureCompliantUmpEndpointName(winrt::hstring const& endpointName) noexcept
    {
        // The specification's limit is a UTF-8 byte count, not a character count, so a name well
        // inside the character limit can still be too long once encoded.
        return winrt::hstring{ internal::TruncateToUtf8ByteCount(
            internal::TrimmedWStringCopy(endpointName.c_str()),
            MIDI_STREAM_MESSAGE_ENDPOINT_NAME_MAX_LENGTH) };
    }

    _Use_decl_annotations_
    winrt::hstring MidiEndpointDeviceHelper::EnsureCompliantProductInstanceId(winrt::hstring const& productInstanceId) noexcept
    {
        // ensures all ASCII characters, and removes any invalid characters for a SWD unique id. Also ensures length is within the spec's
        // requirement of 42 ASCII characters or fewer. If the given string is too long, it is truncated to 42 characters.

        return winrt::hstring{ internal::RemoveInvalidSWDUniqueIdCharacters(productInstanceId.c_str()).substr(0, MIDI_STREAM_MESSAGE_PRODUCT_INSTANCE_ID_MAX_LENGTH) };
    }


    _Use_decl_annotations_
    winrt::hstring MidiEndpointDeviceHelper::GetShortIdFromFullId(winrt::hstring const& fullEndpointDeviceId) noexcept
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
    winrt::hstring MidiEndpointDeviceHelper::GetFullIdFromShortId(winrt::hstring const& shortEndpointDeviceId) noexcept
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
    bool MidiEndpointDeviceHelper::IsPossibleWindowsMidiServicesEndpointDeviceId(winrt::hstring const& fullEndpointDeviceId) noexcept
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
        bool MidiEndpointDeviceHelper::IsPossibleWindowsMidiServicesLegacyApiPortDeviceId(winrt::hstring const& fulllegacyPortDeviceId) noexcept
    {
        try
        {
            auto cleanId = internal::NormalizeEndpointInterfaceIdHStringCopy(fulllegacyPortDeviceId.c_str());

            return cleanId.starts_with(MIDISRV_LEGACY_PORT_SWD_PREFIX) && 
                (cleanId.ends_with(MIDISRV_LEGACY_PORT_DESTINATION_SWD_INTERFACE_SUFFIX) || cleanId.ends_with(MIDISRV_LEGACY_PORT_SOURCE_SWD_INTERFACE_SUFFIX));
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error checking possible Windows MIDI Services MIDI 1 port device id.");
            return false;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception checking possible Windows MIDI Services MIDI 1 port device id.");
            return false;
        }
    }



    _Use_decl_annotations_
    winrt::hstring MidiEndpointDeviceHelper::NormalizeFullId(winrt::hstring const& fullEndpointDeviceId) noexcept
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
