// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#pragma warning (push)
#pragma warning (disable: 4005)

#include <windows.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <format>
#include <tuple>

#include <wrl\module.h>
#include <wrl\event.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\tracelogging.h>
#include <wil\registry.h>
#include <wil\registry_helpers.h>

#include <winmeta.h>
//#include <TraceLoggingProvider.h>

#include "console_tools_shared.h"

#include "wstring_util.h"
#include "resource_util.h"

namespace internal = ::WindowsMidiServicesInternal;

#include "resource.h"

#pragma warning (pop)

const wchar_t* VALUE_NAME_MidisrvTransferComplete = L"MidisrvTransferComplete";
const wchar_t* VALUE_NAME_UseLegacyMidi = L"UseLegacyMidi";


// The format string comes from the string table and uses std::format {0}-style placeholders
// so translators can reorder the substitutions.
template <typename... TArgs>
std::wstring FormatResourceString(_In_ UINT const resourceId, TArgs&&... args)
{
    auto values = std::make_tuple(std::forward<TArgs>(args)...);

    return std::apply(
        [resourceId](auto&... unpacked)
        {
            return std::vformat(internal::ResourceGetWString(resourceId), std::make_wformat_args(unpacked...));
        },
        values);
}



void WriteInfoDetailLine(_In_ std::wstring const& info)
{
    fmt::println(L"- {}", Styled(info, infoTextStyle));
}

void WriteErrorDetailLine(_In_ std::wstring const& info)
{
    fmt::println(L"- {}", Styled(info, errorTextStyle));
}

void WriteBrightLabel(_In_ std::wstring const& label)
{
    auto fullLabel = label + L":";
    fmt::print(L"- {:<25}", Styled(fullLabel, fmt::fg(fmt::color::white)));
}

void WriteLabel(_In_ std::wstring const& label)
{
    auto fullLabel = label + L":";
    fmt::print(L"- {:<25}", Styled(fullLabel, fmt::fg(fmt::color::gray)));
}


#define RETURN_NO_CHANGES_NEEDED        -1
#define RETURN_NO_MIDI_SERVICES         3



bool ValueNeedsReplacing(_In_ std::wstring const& parentKey, _In_ std::wstring const& valueName, _In_ std::wstring const& requiredValue)
{
    try
    {
        auto val = wil::reg::get_value_string(HKEY_LOCAL_MACHINE, parentKey.c_str(), valueName.c_str());

        if (val == requiredValue)
        {
            return false;
        }
    }
    catch (...)
    {
        // missing key/value is a common cause of this
    }

    return true;
}

bool MidisrvTransferCompleteIsPresentAndEnabled(_In_ std::wstring const& parentKey)
{
    try
    {
        auto val = wil::reg::get_value_dword(HKEY_LOCAL_MACHINE, parentKey.c_str(), VALUE_NAME_MidisrvTransferComplete);

        if (val == 1)
        {
            return true;
        }
    }
    catch (...)
    {
        // missing key/value is a common cause of this
    }

    return false;
}


std::vector<std::wstring> CheckRegistryAndGetValuesToDelete(_In_ std::wstring const& key)
{
    std::vector<std::wstring> valuesToDelete{};

    WriteBlankLine();
    WriteInfoLine(FormatResourceString(IDS_STATUS_CHECKING_KEY, key));

    wil::unique_hkey keyForDelete;  // closes when it goes out of scope

    if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, key.c_str(), keyForDelete, wil::reg::key_access::readwrite)))
    {
        for (const auto& value_data : wil::make_range(wil::reg::value_iterator{ keyForDelete.get() }, wil::reg::value_iterator{}))
        {
            std::wstring valueName(value_data.name.begin(), value_data.name.end());

            // Only allowed midi entries:
            // midi : wdmaud.drv
            // midi1 : wdmaud2.drv
            // Other drivers like the Korg BLE driver are allowed, but we need to remove
            // the Korg USB driver from here.
            // midi0 is invalid. All others are unused.

            if ((internal::ToLowerTrimmedWStringCopy(valueName).starts_with(L"midi")) &&
                /*(valueName != "midimapper") && */
                (valueName != VALUE_NAME_MidisrvTransferComplete))
            {
                auto checkValName = valueName;
                checkValName.erase(0, 4);

                if (checkValName.find_first_not_of(L"0123456789") == std::string::npos)
                {
                    std::wstring driverName{};

                    if (value_data.type == REG_SZ)
                    {
                        driverName = wil::reg::get_value_string(HKEY_LOCAL_MACHINE, key.c_str(), value_data.name.c_str());
                        internal::InPlaceToLower(driverName);

                        if (valueName == L"midi" && driverName == L"wdmaud.drv")
                        {
                            WriteInfoDetailLine(FormatResourceString(IDS_FOUND_CORRECT_MIDI_VALUE, driverName));
                        }
                        else if (valueName == L"midi1" && driverName == L"wdmaud2.drv")
                        {
                            WriteInfoDetailLine(FormatResourceString(IDS_FOUND_CORRECT_MIDI1_VALUE, driverName));
                        }
                        else if (valueName == L"midimapper")
                        {
                            WriteInfoDetailLine(FormatResourceString(IDS_FOUND_MIDI_MAPPER, driverName));
                        }
                        else if (driverName == L"korgbm64.drv" && valueName != L"midi" && valueName != L"midi1")
                        {
                            WriteInfoDetailLine(FormatResourceString(IDS_FOUND_KORG_BLE_DRIVER, driverName, valueName));
                        }
                        else if (driverName == L"midimapper.dll" && valueName != L"midi" && valueName != L"midi1")
                        {
                            WriteInfoDetailLine(FormatResourceString(IDS_FOUND_COOLSOFT_MIDI_MAPPER, driverName, valueName));
                        }
                        else if (driverName == L"virtualmidisynth.dll" && valueName != L"midi" && valueName != L"midi1")
                        {
                            WriteInfoDetailLine(FormatResourceString(IDS_FOUND_COOLSOFT_VIRTUAL_MIDI_SYNTH, driverName, valueName));
                        }
                        else
                        {
                            WriteErrorDetailLine(FormatResourceString(IDS_FOUND_INCORRECT_VALUE, valueName, driverName));
                            valuesToDelete.push_back(value_data.name);
                        }
                    }
                    else
                    {
                        WriteErrorDetailLine(FormatResourceString(IDS_FOUND_INCORRECT_VALUE_TYPE, valueName));
                        valuesToDelete.push_back(value_data.name);
                    }

                }
                else
                {
                    // the string starts with "midi" but includes other characters than a number. Leaving it alone.
                    WriteInfoDetailLine(FormatResourceString(IDS_FOUND_OTHER_VALUE, valueName));
                }
            }
        }
    }

    
    if (valuesToDelete.empty())
    {
        WriteInfoLine(internal::ResourceGetWString(IDS_STATUS_NO_VALUES_TO_DELETE));
    }

    WriteBlankLine();

    return valuesToDelete;
}


bool FixMidisrvTransferComplete(_In_ std::wstring const& key)
{
    wil::unique_hkey keyForUpdate;

    if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, key.c_str(), keyForUpdate, wil::reg::key_access::readwrite)))
    {
        return SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_LOCAL_MACHINE, key.c_str(), VALUE_NAME_MidisrvTransferComplete, (DWORD)1));
    }

    return false;
}


bool FixRegistryValues(_In_ std::wstring const& key, _In_ std::vector<std::wstring> valueNamesToDelete, _In_ bool updateMidiValue, _In_ bool updateMidi1Value)
{
    wil::unique_hkey keyForDelete;

    if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, key.c_str(), keyForDelete, wil::reg::key_access::readwrite)))
    {
        for (auto const& valueNameW : valueNamesToDelete)
        {
            auto ret = RegDeleteValue(keyForDelete.get(), valueNameW.c_str());

            if (ret == ERROR_SUCCESS)
            {
                WriteInfoDetailLine(FormatResourceString(IDS_STATUS_DELETED_VALUE, valueNameW));
            }
            else
            {
                WriteErrorDetailLine(FormatResourceString(IDS_ERROR_DELETING_VALUE, valueNameW));
                return false;
            }
        }

        HRESULT hr;

        if (updateMidiValue)
        {
            hr = wil::reg::set_value_string_nothrow(HKEY_LOCAL_MACHINE, key.c_str(), L"midi", L"wdmaud.drv");
            if (!SUCCEEDED(hr))
            {
                WriteErrorDetailLine(internal::ResourceGetWString(IDS_ERROR_UNABLE_TO_WRITE_MIDI_VALUE));
                return false;
            }
            else
            {
                WriteInfoDetailLine(internal::ResourceGetWString(IDS_STATUS_UPDATED_MIDI_VALUE));
            }
        }

        if (updateMidi1Value)
        {
            hr = wil::reg::set_value_string_nothrow(HKEY_LOCAL_MACHINE, key.c_str(), L"midi1", L"wdmaud2.drv");
            if (!SUCCEEDED(hr))
            {
                WriteErrorDetailLine(internal::ResourceGetWString(IDS_ERROR_UNABLE_TO_WRITE_MIDI1_VALUE));
                return false;
            }
            else
            {
                WriteInfoDetailLine(internal::ResourceGetWString(IDS_STATUS_UPDATED_MIDI1_VALUE));
            }
        }



        return true;
    }

    return false;
}


int __cdecl main(int /*argc*/, char* /*argv[]*/)
{
    if (!TrySetConsoleTextMode())
    {
        return RETURN_ERROR_SETTING_CONSOLE_MODE;
    }

    if (!SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)))
    {
        WriteErrorLine(internal::ResourceGetWString(IDS_ERROR_UNABLE_TO_INITIALIZE_COM));
        return RETURN_GENERAL_FAILURE;
    }

    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_TOOL_INFO));
    WriteDoubleSeparatorLine();
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_DESCRIPTION));
    WriteBlankLine();


    // TODO: Change to check API mode

    //if (!CheckForWindowsMidiServices())
    //{
    //    WriteBlankLine();
    //    WriteErrorLine(L"Windows MIDI Services is not present on this PC. No changes will be made.");

    //    return RETURN_NO_MIDI_SERVICES;
    //}

    // check that we're running as admin. Bail if we're not.
    if (!CheckForAdminPermissions())
    {
        WriteBlankLine();
        WriteErrorLine(internal::ResourceGetWString(IDS_ERROR_NOT_ADMINISTRATOR));

        return RETURN_INSUFFICIENT_PERMISSIONS;
    }



    // we shouldn't need to do anything here, because the aliases are no longer used or present
    //std::wstring controlSetMediaRootHklmKey = L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e96c-e325-11ce-bfc1-08002be10318}";

    bool midiValueNeedsReplacing = ValueNeedsReplacing(drivers32HklmKey, L"midi", L"wdmaud.drv");
    bool midi1ValueNeedsReplacing = ValueNeedsReplacing(drivers32HklmKey, L"midi1", L"wdmaud2.drv");

    bool midiWOWValueNeedsReplacing = ValueNeedsReplacing(drivers32WOWHklmKey, L"midi", L"wdmaud.drv");
    bool midi1WOWValueNeedsReplacing = ValueNeedsReplacing(drivers32WOWHklmKey, L"midi1", L"wdmaud2.drv");

    bool midiSrvTransferCompleteFound = MidisrvTransferCompleteIsPresentAndEnabled(drivers32HklmKey);

    std::vector<std::wstring> valuesToDelete64 = CheckRegistryAndGetValuesToDelete(drivers32HklmKey);
    std::vector<std::wstring> valuesToDeleteWOW = CheckRegistryAndGetValuesToDelete(drivers32WOWHklmKey);

    if (!midiSrvTransferCompleteFound || !valuesToDelete64.empty() || !valuesToDeleteWOW.empty() || 
        midiValueNeedsReplacing || midi1ValueNeedsReplacing || 
        midiWOWValueNeedsReplacing || midi1WOWValueNeedsReplacing)
    {
        // Prompt for confirmation

        std::wstring response{};

        WriteBlankLine();
        WriteHighlightLine(internal::ResourceGetWString(IDS_PROMPT_CHANGES_REQUIRED));
        WriteHighlightLine(internal::ResourceGetWString(IDS_PROMPT_CLOSE_OTHER_APPS));
        WriteBlankLine();

        if (!midiSrvTransferCompleteFound || !valuesToDelete64.empty() ||
            midiValueNeedsReplacing || midi1ValueNeedsReplacing)
        {
            WriteHighlightLine(internal::ResourceGetWString(IDS_LABEL_PROPOSED_CHANGES_64_BIT));

            for (auto const& valueNameW : valuesToDelete64)
            {
                WriteInfoDetailLine(FormatResourceString(IDS_PROPOSED_DELETE_VALUE, valueNameW));
            }

            if (midiValueNeedsReplacing)
            {
                WriteInfoDetailLine(internal::ResourceGetWString(IDS_PROPOSED_UPDATE_MIDI_VALUE));
            }

            if (midi1ValueNeedsReplacing)
            {
                WriteInfoDetailLine(internal::ResourceGetWString(IDS_PROPOSED_UPDATE_MIDI1_VALUE));
            }

            if (!midiSrvTransferCompleteFound)
            {
                WriteInfoDetailLine(internal::ResourceGetWString(IDS_PROPOSED_UPDATE_TRANSFER_COMPLETE));
            }

            WriteBlankLine();
        }

        if (!valuesToDeleteWOW.empty() || midiWOWValueNeedsReplacing || midi1WOWValueNeedsReplacing)
        {
            WriteHighlightLine(internal::ResourceGetWString(IDS_LABEL_PROPOSED_CHANGES_32_BIT));

            for (auto const& valueNameW : valuesToDeleteWOW)
            {
                WriteInfoDetailLine(FormatResourceString(IDS_PROPOSED_DELETE_VALUE, valueNameW));
            }

            if (midiWOWValueNeedsReplacing)
            {
                WriteInfoDetailLine(internal::ResourceGetWString(IDS_PROPOSED_UPDATE_MIDI_VALUE));
            }

            if (midi1WOWValueNeedsReplacing)
            {
                WriteInfoDetailLine(internal::ResourceGetWString(IDS_PROPOSED_UPDATE_MIDI1_VALUE));
            }

            WriteBlankLine();
        }

        WriteBlankLine();

        if (PromptForYes(internal::ResourceGetWString(IDS_PROMPT_YES_NO_KEYS)))
        {
            if (!midiSrvTransferCompleteFound || midiValueNeedsReplacing || midi1ValueNeedsReplacing || !valuesToDelete64.empty())
            {
                WriteHighlightLine(internal::ResourceGetWString(IDS_STATUS_MAKING_CHANGES_64_BIT));
                FixRegistryValues(drivers32HklmKey, valuesToDelete64, midiValueNeedsReplacing, midi1ValueNeedsReplacing);

                if (!midiSrvTransferCompleteFound)
                {
                    FixMidisrvTransferComplete(drivers32HklmKey);
                }

                WriteBlankLine();
            }

            if (midiWOWValueNeedsReplacing || midi1WOWValueNeedsReplacing || !valuesToDeleteWOW.empty())
            {
                WriteHighlightLine(internal::ResourceGetWString(IDS_STATUS_MAKING_CHANGES_32_BIT));
                FixRegistryValues(drivers32WOWHklmKey, valuesToDeleteWOW, midiWOWValueNeedsReplacing, midi1WOWValueNeedsReplacing);
                WriteBlankLine();
            }

            // We don't automatically restart audiosrv, audioendpointbuilder, and midisrv here
            // because that could cause problems with other apps which are open. Instead, we
            // ask the user to reboot. It looks lazy, but it's more about not crashing other
            // apps and causing a potential loss of data.

            WriteHighlightLine2(internal::ResourceGetWString(IDS_STATUS_CHANGES_MADE_REBOOT));
            WriteBlankLine();

            return RETURN_SUCCESS;
        }
        else
        {
            WriteHighlightLine(internal::ResourceGetWString(IDS_STATUS_NO_CHANGES_MADE));
            WriteBlankLine();

            return RETURN_USER_ABORTED;
        }
    }
    else
    {
        WriteHighlightLine2(internal::ResourceGetWString(IDS_STATUS_NO_INCORRECT_VALUES));
        WriteBlankLine();

        return RETURN_NO_CHANGES_NEEDED;
    }

}





