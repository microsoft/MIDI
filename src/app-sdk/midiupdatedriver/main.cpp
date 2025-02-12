// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include "pch.h"

#include "color.hpp"

#define LINE_LENGTH 79

#define CMD_RETURN_SUCCESS                  0
#define CMD_RETURN_SUCCESS_NEEDS_REBOOT     1

#define CMD_RETURN_INSUFFICIENT_PERMISSIONS 2
#define CMD_RETURN_INVALID_OR_MISSING_ARGS  3
#define CMD_RETURN_INVALID_DEVICE           4
#define CMD_RETURN_DRIVER_ALREADY_ASSIGNED  5

#define CMD_RETURN_ERROR_OTHER_FAILURE      9


void WriteInfo(_In_ std::string info)
{
    std::cout << dye::aqua(info) << std::endl;
}

void WriteSuccess(_In_ std::string info)
{
    std::cout << dye::green(info) << std::endl;
}

void WriteError(_In_ std::string error)
{
    std::cout << dye::light_red(error) << std::endl;
}

void WriteSystemError(_In_ DWORD errorNumber)
{
    WCHAR message[MAX_ERROR_MESSAGE_CHARS]{ 0 };
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, errorNumber, 0, message, MAX_ERROR_MESSAGE_CHARS, NULL);
    std::wcout << errorNumber << " : " << message << std::endl;
}



HDEVINFO m_deviceInfoSet;
SP_DEVINFO_DATA m_device{ };

HDEVINFO m_driverInfoSet;
SP_DRVINFO_DATA m_newDriverInfo{ };


bool LoadDeviceInfo(_In_ std::wstring deviceInstanceId)
{
    auto handle = SetupDiGetClassDevsW(&GUID_CLASS_USB_DEVICE, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);

    if (handle == INVALID_HANDLE_VALUE)
    {
        WriteError("Unable to load device information.");
        WriteSystemError(GetLastError());
        std::wcout << deviceInstanceId << std::endl;

        return false;
    }

    //std::cout << "debug: got device info set" << std::endl;

    m_deviceInfoSet = handle;
    m_device.cbSize = sizeof(SP_DEVINFO_DATA);

    BOOL enumResult{ TRUE };
    DWORD memberIndex{ 0 };
    while (enumResult)
    {
        enumResult = SetupDiEnumDeviceInfo(m_deviceInfoSet, memberIndex, &m_device);

        if (!enumResult)
        {
            WriteError("Unable to enum device information.");
            WriteSystemError(GetLastError());
            std::wcout << deviceInstanceId << std::endl;

            return false;
        }
        else
        {
            wchar_t thisDeviceInstanceId[512]{ 0 };
            auto res = SetupDiGetDeviceInstanceIdW(m_deviceInfoSet, &m_device, thisDeviceInstanceId, 512, NULL);

            //std::wcout << thisDeviceInstanceId << std::endl;

            if (res)
            {
                if (internal::ToUpperTrimmedWStringCopy(thisDeviceInstanceId) == deviceInstanceId)
                {
                    std::wcout << L"Found a match" << std::endl;
                    return true;
                }
            }
        }

        memberIndex++;
    }

}

bool LoadMidi2DriverInfo()
{
    //auto handle = SetupDiGetClassDevsW(&GUID_CLASS_USB_DEVICE, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);

    //GUID classList[1024];
    //DWORD requiredSize{ 0 };

    //auto result = SetupDiBuildClassInfoList(
    //    DIBCI_NOINSTALLCLASS,
    //    classList,
    //    1024,
    //    &requiredSize
    //);
    
    auto result = SetupDiBuildDriverInfoList(
        m_deviceInfoSet,
        NULL,
        SPDIT_CLASSDRIVER
    );


    if (!result)
    {
        WriteError("Unable to build class driver list.");
        WriteSystemError(GetLastError());

        return false;
    }

    BOOL enumResult{ TRUE };
    DWORD memberIndex{ 0 };

    SP_DRVINFO_DATA driverData{ };


    SP_DEVINSTALL_PARAMS_W installParams{};
    installParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS_W);
    installParams.Flags = DI_COMPAT_FROM_CLASS;

    SetupDiSetDeviceInstallParamsW(m_deviceInfoSet, &m_device, &installParams);

    while (enumResult)
    {
        driverData.cbSize = sizeof(SP_DRVINFO_DATA);
        enumResult = SetupDiEnumDriverInfoW(m_deviceInfoSet, &m_device, SPDIT_CLASSDRIVER, memberIndex, &driverData);


        if (!enumResult)
        {
            WriteError("Unable to enum driver information.");
            WriteSystemError(GetLastError());

            return false;
        }
        else
        {
            //if (driverData.MfgName == L"Microsoft")
            {
                std::wcout << L"---" << std::endl;
                std::wcout << driverData.MfgName << std::endl;
                std::wcout << driverData.ProviderName << std::endl;
                std::wcout << driverData.Description << std::endl;
            }
        }

        memberIndex++;
    }


    // media class: {4d36e96c-e325-11ce-bfc1-08002be10318}

    return true;
}

int ChangeDeviceFromMidi1ToMidi2(_In_ std::wstring deviceInstanceId)
{
    BOOL needsReboot{ false };

    if (!LoadDeviceInfo(deviceInstanceId))
    {
        return CMD_RETURN_INVALID_DEVICE;
    }
    

    if (!LoadMidi2DriverInfo())
    {
        return CMD_RETURN_ERROR_OTHER_FAILURE;
    }





    auto ret = TRUE;
    //auto ret = DiInstallDevice(
    //    NULL,
    //    m_deviceInfoSet,
    //    &m_device,
    //    &m_newDriverInfo,
    //    0,
    //    &needsReboot
    //);


    //auto ret = UpdateDriverForPlugAndPlayDevicesW(
    //    NULL,
    //    deviceInstanceId.c_str(),
    //    L"C:\\Windows\\System32\\DriverStore\\FileRepository\\usbmidi2.inf_amd64_5518a95d6f5d2db5\\usbmidi2.inf",
    //    INSTALLFLAG_FORCE | INSTALLFLAG_READONLY,
    //    &needsReboot
    //);


    if (ret == TRUE)
    {
        if (needsReboot)
        {
            WriteSuccess("Driver assignment succeeded. Please reboot to finalize.");
            return CMD_RETURN_SUCCESS_NEEDS_REBOOT;
        }
        else
        {
            WriteSuccess("Driver assignment succeeded.");
            return CMD_RETURN_SUCCESS;
        }
        
    }
    else if (ret == ERROR_ACCESS_DENIED)
    {
        WriteError("Insufficient permissions. This must be run from an elevated Administrator process.");
        return CMD_RETURN_INSUFFICIENT_PERMISSIONS;
    }
    else
    {
        // some other error
        WriteSystemError(GetLastError());
        WriteError("Unknown error. Unable to assign driver.");
        return CMD_RETURN_ERROR_OTHER_FAILURE;
    }

    return CMD_RETURN_SUCCESS;
}

int ChangeDeviceFromMidi2ToMidi1(_In_ std::wstring deviceInstanceId)
{

    return CMD_RETURN_SUCCESS;
}

int ChangeDeviceToDefault(_In_ std::wstring deviceInstanceId)
{

    return CMD_RETURN_SUCCESS;
}


int __cdecl wmain(_In_ int argc, _In_ wchar_t* argv[])
{
    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
    std::cout << dye::aqua(" Switch a class-compliant USB MIDI device to a new class driver") << std::endl;
    std::cout << dye::aqua(" This must be run in an elevated (Administrator) command prompt") << std::endl;
    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;

    // enter number to monitor

    uint16_t portNumber{ 0 };
    bool portNumberProvided{ false };

    if (argc == 3)
    {
        std::wstring targetVersion = internal::ToLowerTrimmedWStringCopy(argv[1]);
        std::wstring deviceInstanceId = internal::ToUpperTrimmedWStringCopy(argv[2]);

        if (targetVersion != L"1" && targetVersion != L"2" && targetVersion != L"default")
        {
            WriteError("Invalid driver version specified.");

            return CMD_RETURN_INVALID_OR_MISSING_ARGS;
        }

        //if (!IsValidDevice(deviceInstanceId))
        //{
        //    WriteError("Invalid device instance id specified.");

        //    return CMD_RETURN_INVALID_DEVICE;
        //}

        // data checks out. proceed.

        if (targetVersion == L"1")
        {
            return ChangeDeviceFromMidi2ToMidi1(deviceInstanceId);
        }
        else if(targetVersion == L"2")
        {
            return ChangeDeviceFromMidi1ToMidi2(deviceInstanceId);
        }
        else // default
        {
            return ChangeDeviceToDefault(deviceInstanceId);
        }

    }
    else
    {
        std::string tab{ "    " };

        std::cout << std::endl;
        std::cout << dye::light_yellow("USAGE:") << std::endl;
        std::cout << tab << "midiupdatedriver" << dye::grey(" <driver version> <device instance id>") << std::endl;
        std::cout << std::endl;
        std::cout << dye::light_yellow("EXAMPLES:") << std::endl;
        std::cout << tab << "midiupdatedriver     " << dye::grey("1 \"USB\\VID_0944&PID_0133\\7&2eb6a416&0&4\"") << std::endl;
        std::cout << tab << "midiupdatedriver     " << dye::grey("2 \"USB\\VID_2E14&PID_0006&MI_02\\8&28675245&0&0002\"") << std::endl;
        std::cout << tab << "midiupdatedriver     " << dye::grey("default \"USB\\VID_2E14&PID_0006&MI_02\\8&28675245&0&0002\"") << std::endl;
        std::cout << std::endl;
        std::cout << dye::light_yellow("ARGUMENTS:") << std::endl;
        std::cout << tab << "<driver version>     " << dye::grey("1 for usbaudio.sys (MIDI 1 class driver)") << std::endl;
        std::cout << tab << "                     " << dye::grey("2 for USBMidi2.sys (MIDI 1 / 2 class driver)") << std::endl;
        std::cout << tab << "                     " << dye::grey("default to let Windows pick the best driver for the device") << std::endl;
        std::cout << tab << "<device instance id> " << dye::grey("The unique Instance Id found using pnputil or the device properties") << std::endl;
        std::cout << std::endl;

        return CMD_RETURN_INVALID_OR_MISSING_ARGS;
    }


    return CMD_RETURN_SUCCESS;
}

