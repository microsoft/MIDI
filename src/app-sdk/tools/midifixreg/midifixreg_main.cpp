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


#define LINE_LENGTH 80

void WriteBlankLine()
{
    std::cout << std::endl;
}

void WriteDoubleLineSeparator()
{
    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
}

void WriteInfo(std::string info)
{
    std::cout << dye::aqua(info) << std::endl;
}

void WriteInfo(std::wstring info)
{
    std::cout << hue::aqua;
    std::wcout << info;
    std::cout << hue::reset << std::endl;
}

void WriteImportant(std::string info)
{
    std::cout << dye::light_aqua(info) << std::endl;
}

void WriteImportant(std::wstring info)
{
    std::cout << hue::light_aqua;
    std::wcout << info;
    std::cout << hue::reset << std::endl;
}


void WriteSuperImportant(std::string info)
{
    std::cout << dye::light_yellow(info) << std::endl;
}


void WriteError(std::string error)
{
    std::cout << dye::light_red(error) << std::endl;
}

void WriteBrightLabel(std::string label)
{
    auto fullLabel = label + ":";
    std::cout << std::left << std::setw(25) << std::setfill(' ') << dye::white(fullLabel);
}

void WriteLabel(std::string label)
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

bool ValueNeedsReplacing(std::wstring parentKey, std::wstring valueName, std::wstring requiredValue)
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






int __cdecl main(int /*argc*/, char* /*argv[]*/)
{
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

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

    // we shouldn't need to do anything here, because the aliases are no longer used or present
    //std::wstring controlSetMediaRootHklmKey = L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e96c-e325-11ce-bfc1-08002be10318}";

    bool midiValueNeedsReplacing{ false };
    bool midi1ValueNeedsReplacing{ false };

    // TODO: Move the code to replace these into functions, and also run after user confirmation

    HRESULT hr{};

    midiValueNeedsReplacing = ValueNeedsReplacing(drivers32HklmKey, L"midi", L"wdmaud.drv");
    midi1ValueNeedsReplacing = ValueNeedsReplacing(drivers32HklmKey, L"midi1", L"wdmaud2.drv");

    std::vector<std::wstring> valuesToDelete;

    wil::unique_hkey keyForDelete;
    if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, drivers32HklmKey.c_str(), keyForDelete, wil::reg::key_access::readwrite)))
    {
        for (const auto& value_data : wil::make_range(wil::reg::value_iterator{ keyForDelete.get() }, wil::reg::value_iterator{}))
        {
            std::wstring valueName(value_data.name.begin(), value_data.name.end());

            // Only allowed midi entries:
            // midi : wdmaud.drv
            // midi1 : wdmaud2.drv
            // midi0 is invalid. All others are unused.

            // we're going scorched earth here. 3P drivers shouldn't be listed anymore, so we simply remove them all and
            // create entries only for wdmaud.drv and wdmaud2.drv

            if ((valueName.starts_with(L"midi")) &&
                /*(valueName != "midimapper") && */
                (valueName != L"MidisrvTransferComplete"))
            {
                auto checkValName = valueName;
                checkValName.erase(0, 4);

                if (checkValName.find_first_not_of(L"0123456789") == std::string::npos)
                {
                    if (value_data.type == REG_SZ)
                    {
                        auto valueDataW = wil::reg::get_value_string(HKEY_LOCAL_MACHINE, drivers32HklmKey.c_str(), value_data.name.c_str());

                        WriteImportant(L"- Found value '" + valueName + L"' with value '" + valueDataW + L"'");
                    }
                    else
                    {
                        WriteImportant(L"- Found value '" + valueName + L"' with incorrect value type.");
                    }

                    // we handle these two differently
                    if (valueName != L"midi" && valueName != L"midi1")
                    {
                        valuesToDelete.push_back(value_data.name);
                    }
                }
                else
                {
                    // the string starts with "midi" but includes other characters than a number. Leaving it alone.
                    WriteInfo(L"- Found value '" + valueName + L"'. Leaving it alone.");
                }

            }
        }

        if (valuesToDelete.size() > 0 || midiValueNeedsReplacing || midi1ValueNeedsReplacing)
        {
            // Prompt for confirmation

            std::wstring response{};

            WriteBlankLine();
            WriteInfo("You only need to continue if you've used various apps to remove Korg or other MIDI 1.0");
            WriteInfo("drivers from your system, and as a result no longer see MIDI ports in MIDI 1.0 apps.");
            WriteBlankLine();
            WriteSuperImportant("Before continuing, please close any other apps which use MIDI, and save your work.");
            WriteBlankLine();

            WriteImportant("Proposed changes:");
            for (auto const& valueNameW : valuesToDelete)
            {
                std::string valueName(valueNameW.begin(), valueNameW.end());
                WriteImportant("- Delete registry value '" + valueName + "'");
            }

            if (midiValueNeedsReplacing)
            {
                WriteImportant("- Update registry value 'midi' to wdmaud.drv");
            }

            if (midi1ValueNeedsReplacing)
            {
                WriteImportant("- Update registry value 'midi1' to wdmaud2.drv");
            }

            WriteBlankLine();

            std::cout << "Enter 'y' to make the proposed changes, or 'n' to exit. ";
            std::wcin >> response;

            if (internal::ToLowerTrimmedWStringCopy(response) == L"y")
            {
                WriteBlankLine();
                WriteImportant("Making changes... ");
                // actually delete the values now

                for (auto const& valueNameW : valuesToDelete)
                {
                    std::string valueName(valueNameW.begin(), valueNameW.end());

                    auto ret = RegDeleteValue(keyForDelete.get(), valueNameW.c_str());

                    if (ret == ERROR_SUCCESS)
                    {
                        WriteInfo("- Deleted value '" + valueName + "'");
                    }
                    else
                    {
                        WriteError("- Error deleting value '" + valueName + "'");
                    }
                }

                if (midiValueNeedsReplacing)
                {
                    hr = wil::reg::set_value_string_nothrow(HKEY_LOCAL_MACHINE, drivers32HklmKey.c_str(), L"midi", L"wdmaud.drv");
                    if (!SUCCEEDED(hr))
                    {
                        WriteError("- Unable to write 'midi' value of 'wdmaud.drv'");
                        RETURN_INSUFFICIENT_PERMISSIONS;
                    }
                    else
                    {
                        WriteInfo("- Updated 'midi' to 'wdmaud.drv'");
                    }
                }

                if (midi1ValueNeedsReplacing)
                {
                    hr = wil::reg::set_value_string_nothrow(HKEY_LOCAL_MACHINE, drivers32HklmKey.c_str(), L"midi1", L"wdmaud2.drv");
                    if (!SUCCEEDED(hr))
                    {
                        WriteError("- Unable to write 'midi1' value of 'wdmaud2.drv'");
                        RETURN_INSUFFICIENT_PERMISSIONS;
                    }
                    else
                    {
                        WriteInfo("- Updated 'midi1' to 'wdmaud2.drv'");
                    }
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
            WriteImportant("No incorrect values found.");
        }
    }
    else
    {
        WriteError("Unable to open reg key for delete.");
    }

    WriteBlankLine();


    RETURN_SUCCESS;
}





