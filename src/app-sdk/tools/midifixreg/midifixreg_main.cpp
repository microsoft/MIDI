// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
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

#include "wstring_util.h"

namespace internal = ::WindowsMidiServicesInternal;

#pragma warning (pop)

#pragma warning(push)
#pragma warning(disable: 4244)
#include "color.hpp"
#pragma warning(pop)

const wchar_t* VALUE_NAME_MidisrvTransferComplete = L"MidisrvTransferComplete";

#define LINE_LENGTH 80

void WriteBlankLine()
{
    std::cout << std::endl;
}

void WriteDoubleLineSeparator()
{
    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
}

void WriteInfo(_In_ std::string info)
{
    std::cout << dye::aqua(info) << std::endl;
}

void WriteInfo(_In_ std::wstring info)
{
    std::cout << hue::aqua;
    std::wcout << info;
    std::cout << hue::reset << std::endl;
}

void WriteInfoDetail(_In_ std::wstring info)
{
    std::cout << hue::white;
    std::wcout << L"- " << info;
    std::cout << hue::reset << std::endl;
}

void WriteErrorDetail(_In_ std::wstring info)
{
    std::cout << hue::light_red;
    std::wcout << L"- " << info;
    std::cout << hue::reset << std::endl;
}

void WriteImportant(_In_ std::string info)
{
    std::cout << dye::light_aqua(info) << std::endl;
}

void WriteImportant(_In_ std::wstring info)
{
    std::cout << hue::light_aqua;
    std::wcout << info;
    std::cout << hue::reset << std::endl;
}


void WriteSuperImportant(_In_ std::string info)
{
    std::cout << dye::light_yellow(info) << std::endl;
}


void WriteError(_In_ std::string error)
{
    std::cout << dye::light_red(error) << std::endl;
}

void WriteBrightLabel(_In_ std::string label)
{
    auto fullLabel = label + ":";
    std::cout << std::left << std::setw(25) << std::setfill(' ') << dye::white(fullLabel);
}

void WriteLabel(_In_ std::string label)
{
    auto fullLabel = label + ":";
    std::cout << std::left << std::setw(25) << std::setfill(' ') << dye::grey(fullLabel);
}


#define RETURN_SUCCESS                  return 0
#define RETURN_INSUFFICIENT_PERMISSIONS return 1
#define RETURN_USER_ABORTED             return 2
#define RETURN_NO_MIDI_SERVICES         return 3

struct __declspec(uuid("2BA15E4E-5417-4A66-85B8-2B2260EFBC84")) MidiSrvTransportPlaceholder : ::IUnknown
{
};

bool CheckForWindowsMidiServices()
{
    // check for Windows MIDI Services (create the midisrvtransport COM object). If it fails, we do not
    // have Windows MIDI Services installed, and will not make any changes.

    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // TODO: This succeeds during controlled rollout even when Windows MIDI Services is not yet enabled
    // Needs to be changed to latest guidance


    wil::com_ptr_nothrow<IUnknown> servicePointer;

    auto hr = CoCreateInstance(
        __uuidof(MidiSrvTransportPlaceholder),
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&servicePointer)
    );

    if (SUCCEEDED(hr))
    {
        return true;
    }

    return false;
}

bool CheckForAdminPermissions()
{
    bool elevated{ false };

    HANDLE tokenHandle{ nullptr };
    TOKEN_ELEVATION elevation;
    DWORD tokenInfoSize{ 0 };

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tokenHandle))
    {
        if (GetTokenInformation(tokenHandle, TokenElevation, &elevation, sizeof(elevation), &tokenInfoSize))
        {
            elevated = elevation.TokenIsElevated;
        }
    }

    if (tokenHandle)
    {
        CloseHandle(tokenHandle);
    }


    return elevated;
}

bool ValueNeedsReplacing(_In_ std::wstring parentKey, _In_ std::wstring valueName, _In_ std::wstring requiredValue)
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

bool MidisrvTransferCompleteIsPresentAndEnabled(_In_ std::wstring parentKey)
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


std::vector<std::wstring> CheckRegistryAndGetValuesToDelete(_In_ std::wstring key)
{
    std::vector<std::wstring> valuesToDelete{};

    WriteBlankLine();
    WriteInfo(L"Checking '" + key + L"'.");

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
                            WriteInfoDetail(L"Found correct required 'midi' value '" + driverName + L"'. Leaving it alone.");
                        }
                        else if (valueName == L"midi1" && driverName == L"wdmaud2.drv")
                        {
                            WriteInfoDetail(L"Found correct required 'midi1' value '" + driverName + L"'. Leaving it alone.");
                        }
                        else if (valueName == L"midimapper")
                        {
                            WriteInfoDetail(L"Found 'midimapper' with value '" + driverName + L"'. Leaving it alone.");
                        }
                        else if (driverName == L"korgbm64.drv" && valueName != L"midi" && valueName != L"midi1")
                        {
                            WriteInfoDetail(L"Found KORG BLE driver '" + driverName + L"' in '" + valueName + L"', which is fine. Leaving it alone.");
                        }
                        else if (driverName == L"midimapper.dll" && valueName != L"midi" && valueName != L"midi1")
                        {
                            WriteInfoDetail(L"Found CoolSoft MIDI Mapper '" + driverName + L"' in '" + valueName + L"', which is fine. Leaving it alone.");
                        }
                        else if (driverName == L"virtualmidisynth.dll" && valueName != L"midi" && valueName != L"midi1")
                        {
                            WriteInfoDetail(L"Found CoolSoft Virtual MIDI Synth '" + driverName + L"' in '" + valueName + L"', which is fine. Leaving it alone.");
                        }
                        else
                        {
                            WriteErrorDetail(L"Found value '" + valueName + L"' with value '" + driverName + L"'");
                            valuesToDelete.push_back(value_data.name);
                        }
                    }
                    else
                    {
                        WriteErrorDetail(L"Found value '" + valueName + L"' with incorrect value type.");
                        valuesToDelete.push_back(value_data.name);
                    }

                }
                else
                {
                    // the string starts with "midi" but includes other characters than a number. Leaving it alone.
                    WriteInfoDetail(L"Found value '" + valueName + L"'. Leaving it alone.");
                }
            }
        }
    }

    
    if (valuesToDelete.empty())
    {
        WriteInfo(L"No values require deleting from this location.");
    }

    WriteBlankLine();

    return valuesToDelete;
}


bool FixMidisrvTransferComplete(_In_ std::wstring key)
{
    wil::unique_hkey keyForUpdate;

    if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, key.c_str(), keyForUpdate, wil::reg::key_access::readwrite)))
    {
        return SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_LOCAL_MACHINE, key.c_str(), VALUE_NAME_MidisrvTransferComplete, (DWORD)1));
    }

    return false;
}


bool FixRegistryValues(_In_ std::wstring key, _In_ std::vector<std::wstring> valueNamesToDelete, _In_ bool updateMidiValue, _In_ bool updateMidi1Value)
{
    wil::unique_hkey keyForDelete;

    if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, key.c_str(), keyForDelete, wil::reg::key_access::readwrite)))
    {
        for (auto const& valueNameW : valueNamesToDelete)
        {
            auto ret = RegDeleteValue(keyForDelete.get(), valueNameW.c_str());

            if (ret == ERROR_SUCCESS)
            {
                WriteInfoDetail(L"Deleted registry value '" + valueNameW + L"'");
            }
            else
            {
                WriteErrorDetail(L"Error deleting value '" + valueNameW + L"'");
                return false;
            }
        }

        HRESULT hr;

        if (updateMidiValue)
        {
            hr = wil::reg::set_value_string_nothrow(HKEY_LOCAL_MACHINE, key.c_str(), L"midi", L"wdmaud.drv");
            if (!SUCCEEDED(hr))
            {
                WriteErrorDetail(L"Unable to write 'midi' value of 'wdmaud.drv'");
                return false;
            }
            else
            {
                WriteInfoDetail(L"Updated registry value 'midi' to 'wdmaud.drv'");
            }
        }

        if (updateMidi1Value)
        {
            hr = wil::reg::set_value_string_nothrow(HKEY_LOCAL_MACHINE, key.c_str(), L"midi1", L"wdmaud2.drv");
            if (!SUCCEEDED(hr))
            {
                WriteErrorDetail(L"Unable to write 'midi1' value of 'wdmaud2.drv'");
                return false;
            }
            else
            {
                WriteInfoDetail(L"Updated registry value 'midi1' to 'wdmaud2.drv'");
            }
        }



        return true;
    }

    return false;
}


int __cdecl main(int /*argc*/, char* /*argv[]*/)
{
    if (!SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)))
    {
        WriteError("Unable to initialize COM.");
        return -1;
    }

    WriteDoubleLineSeparator();
    WriteInfo("This tool is part of the Windows MIDI Services SDK and tools");
    WriteInfo("Copyright 2026- Microsoft Corporation.");
    WriteInfo("Information, license, and source available at https://aka.ms/midi");
    WriteDoubleLineSeparator();

    if (!CheckForWindowsMidiServices())
    {
        WriteBlankLine();
        WriteError("Windows MIDI Services is not present on this PC. No changes will be made.");

        RETURN_NO_MIDI_SERVICES;
    }

    // check that we're running as admin. Bail if we're not.
    if (!CheckForAdminPermissions())
    {
        WriteBlankLine();
        WriteError("Access Denied. Administrator permissions are needed to modify the registry entries. Use an administrator command prompt to run this utility.");

        RETURN_INSUFFICIENT_PERMISSIONS;
    }

    std::wstring drivers32HklmKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Drivers32";
    std::wstring drivers32WOWHklmKey = L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows NT\\CurrentVersion\\Drivers32";

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
        WriteSuperImportant("Changes are required for Windows MIDI Services to work correctly with older applications.");
        WriteSuperImportant("Before continuing, please close any other apps which use MIDI, and save your work.");
        WriteBlankLine();

        if (!midiSrvTransferCompleteFound || !valuesToDelete64.empty() ||
            midiValueNeedsReplacing || midi1ValueNeedsReplacing)
        {
            WriteImportant("Proposed changes for 64 bit app location:");

            for (auto const& valueNameW : valuesToDelete64)
            {
                WriteInfoDetail(L"Delete registry value '" + valueNameW + L"'");
            }

            if (midiValueNeedsReplacing)
            {
                WriteInfoDetail(L"Update registry value 'midi' to 'wdmaud.drv'");
            }

            if (midi1ValueNeedsReplacing)
            {
                WriteInfoDetail(L"Update registry value 'midi1' to 'wdmaud2.drv'");
            }

            if (!midiSrvTransferCompleteFound)
            {
                WriteInfoDetail(L"Update registry value 'MidisrvTransferComplete' to '1'");
            }

            WriteBlankLine();
        }

        if (!valuesToDeleteWOW.empty() || midiWOWValueNeedsReplacing || midi1WOWValueNeedsReplacing)
        {
            WriteImportant("Proposed changes for 32 bit app location:");
            for (auto const& valueNameW : valuesToDeleteWOW)
            {
                WriteInfoDetail(L"Delete registry value '" + valueNameW + L"'");
            }

            if (midiWOWValueNeedsReplacing)
            {
                WriteInfoDetail(L"Update registry value 'midi' to wdmaud.drv");
            }

            if (midi1WOWValueNeedsReplacing)
            {
                WriteInfoDetail(L"Update registry value 'midi1' to wdmaud2.drv");
            }

            WriteBlankLine();
        }

        std::cout << "Please enter 'y' to make the proposed changes, or 'n' to exit. ";
        std::wcin >> response;

        WriteBlankLine();

        if (internal::ToLowerTrimmedWStringCopy(response) == L"y")
        {
            if (!midiSrvTransferCompleteFound || midiValueNeedsReplacing || midi1ValueNeedsReplacing || !valuesToDelete64.empty())
            {
                WriteImportant("Making required changes for 64 bit apps");
                FixRegistryValues(drivers32HklmKey, valuesToDelete64, midiValueNeedsReplacing, midi1ValueNeedsReplacing);

                if (!midiSrvTransferCompleteFound)
                {
                    FixMidisrvTransferComplete(drivers32HklmKey);
                }

                WriteBlankLine();
            }

            if (! midiWOWValueNeedsReplacing || midi1WOWValueNeedsReplacing || !valuesToDeleteWOW.empty())
            {
                WriteImportant("Making required changes for 32 bit apps");
                FixRegistryValues(drivers32WOWHklmKey, valuesToDeleteWOW, midiWOWValueNeedsReplacing, midi1WOWValueNeedsReplacing);
                WriteBlankLine();
            }

            // We don't automatically restart audiosrv, audioendpointbuilder, and midisrv here
            // because that could cause problems with other apps which are open. Instead, we
            // ask the user to reboot. It looks lazy, but it's more about not crashing other
            // apps and causing a potential loss of data.

            WriteSuperImportant("Changes made. Please reboot your PC.");
        }
        else
        {
            WriteImportant("No changes made.");
        }
    }
    else
    {
        WriteSuperImportant("No incorrect values found. This part of the registry seems fine.");
    }

    WriteBlankLine();

    RETURN_SUCCESS;
}





