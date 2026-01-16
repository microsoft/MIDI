// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include "pch.h"

#pragma warning(push)
#pragma warning(disable: 4244)
#include "color.hpp"
#pragma warning(pop)

#include <boost\program_options.hpp>
namespace po = boost::program_options;

// RPC code that complicates this source.
#include "midicheckservice_rpc.h"

// return codes from the cmdline
#include "midicheckservice_return_codes.h"

bool m_optionQuiet{ false };


void WriteHeading(_In_ std::string info)
{
    if (m_optionQuiet) return;

    std::cout << std::endl << dye::aqua(info) << std::endl;
}

void WriteInfo(_In_ std::string info)
{
    if (m_optionQuiet) return;

    std::cout << " - " << dye::white(info) << std::endl;
}

void WriteError(_In_ std::string info)
{
    if (m_optionQuiet) return;

    std::cout << " - " << dye::red(info) << std::endl;
}

#define LINE_LENGTH 96



po::options_description m_optionsDesc{ "\nThis utility checks to see if Windows MIDI Services is installed.\n\nOptions" };
po::variables_map m_optionsMap;

#define OPTION_HELP_SHORT       "h"
#define OPTION_HELP_LONG        "help"
#define OPTION_HELP_FULL        OPTION_HELP_LONG "," OPTION_HELP_SHORT

#define OPTION_QUIET_SHORT      "q"
#define OPTION_QUIET_LONG       "quiet"
#define OPTION_QUIET_FULL       OPTION_QUIET_LONG "," OPTION_QUIET_SHORT


void DisplayHelp()
{
    std::cout << m_optionsDesc << std::endl;
}


void SetupOptions()
{
    m_optionsDesc.add_options()
            (OPTION_HELP_FULL, "Show help")
            (OPTION_QUIET_FULL, "Return a value but do not produce any output")
            ;
}

// returns true if the program should continue running
bool ParseCommandLine(_In_ int argc, _In_ WCHAR* argv[])
{
    po::store(po::parse_command_line(argc, argv, m_optionsDesc), m_optionsMap);
    po::notify(m_optionsMap);

    if (m_optionsMap.count(OPTION_HELP_LONG))
    {
        DisplayHelp();

        return false;
    }

    if (m_optionsMap.count(OPTION_QUIET_LONG))
    {
        m_optionQuiet = true;
    }



    return true;
}


void ShutdownMidisrv()
{
    WriteInfo("Shutting down the MIDI service, if present. You may receive an elevation prompt for this action.");

    system("pause");

    ShellExecute(
        NULL,
        L"runas",
        L"net",
        L"stop midisrv",
        NULL,
        SW_HIDE
    );

}

bool GetMidisrvPathName(_In_ std::wstring& pathName)
{
    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hService = NULL;
    LPQUERY_SERVICE_CONFIGW lpServiceConfig = NULL;
    DWORD dwBytesNeeded = 0;

    std::wstring serviceName { L"midisrv" };

    auto cleanup = wil::scope_exit([&]()
    {
        if (lpServiceConfig)
        {
            LocalFree(lpServiceConfig);
        }

        if (hService)
        {
            CloseServiceHandle(hService);
        }

        if (hSCManager)
        {
            CloseServiceHandle(hSCManager);
        }

    });


    // Open the Service Control Manager
    hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCManager) 
    {
        return false;
    }

    // Open the service
    hService = OpenServiceW(hSCManager, serviceName.c_str(), SERVICE_QUERY_CONFIG);
    if (!hService) 
    {
        return false;
    }

    if (!QueryServiceConfigW(hService, NULL, 0, &dwBytesNeeded))
    {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) 
        {
            return false;
        }
    }
    
    lpServiceConfig =  (LPQUERY_SERVICE_CONFIGW)LocalAlloc(LPTR, dwBytesNeeded);
    if (!lpServiceConfig) 
    {
        return false;
    }

    if (!QueryServiceConfigW(hService, lpServiceConfig, dwBytesNeeded, &dwBytesNeeded)) 
    {
        return false;
    }

    pathName.assign(lpServiceConfig->lpBinaryPathName);

    return true;
}

// this is a simple function for use only in this tool. It's not to be used for any sort
// of validation which has security implications
bool ValidateInBoxMidisrvPath()
{
    std::wstring pathName{};
    std::wstring system32{};


    if (GetMidisrvPathName(pathName))
    {
        system32.resize(MAX_PATH);

        auto ret = GetSystemDirectoryW(system32.data(), MAX_PATH);

        if (ret == 0)
        {
            // couldn't get System32 folder
            return false;
        }

        system32.resize(ret);

        if (system32[0] == '\"' && system32[system32.size()-1] == '\"')
        {
            // remove leading and trailing quotes
            system32 = system32.substr(1, system32.size()-2);
        }

        if (pathName[0] == '\"' && pathName[pathName.size()-1] == '\"')
        {
            // remove leading and trailing quotes
            pathName = pathName.substr(1, pathName.size() - 2);
        }


        pathName = WindowsMidiServicesInternal::ToLowerTrimmedWStringCopy(pathName);
        system32 = WindowsMidiServicesInternal::ToLowerTrimmedWStringCopy(system32);


        if ((!pathName.empty() && !system32.empty()) && 
            (pathName.starts_with(system32) || pathName.starts_with(L"\"" + system32 + L"\""))
            )
        {
            // path name starts with system32 location, so this is very likely the in-box service
            return true;
        }
        else
        {
            return false;
        }

    }

    return false;
}


bool VerifyWdmaud2Registry()
{
    std::wstring drivers32HklmKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Drivers32";

    
    for (int i = 0; i < 10; i++)
    {
        std::wstring valueName = (i == 0 ? L"midi" : L"midi" + std::to_wstring(i));

        try
        {
            auto val = wil::reg::get_value_string(HKEY_LOCAL_MACHINE, drivers32HklmKey.c_str(), valueName.c_str());

            if (val == L"wdmaud2.drv")
            {
                return true;
            }
        }
        catch(...)
        {
            //WriteError(L"Unable to open value " + valueName);
        }

    }

    return false;
}




int __cdecl wmain(_In_ int argc, _In_ WCHAR* argv[])
{
    // this also initializes COM
    winrt::init_apartment();

    SetupOptions();
    
    if (!ParseCommandLine(argc, argv))
    {
        // user picked help or some other option which skips evaluation
        return static_cast<int>(MIDISRV_CHECK_RETURN_VALUE_CHECK_SKIPPED);
    }

    if (!m_optionQuiet)
    {
        std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
        std::cout << dye::aqua(" This tool is part of the Windows MIDI Services SDK and tools") << std::endl;
        std::cout << dye::aqua(" Copyright 2025- Microsoft Corporation.") << std::endl;
        std::cout << dye::aqua(" Information, license, and source available at https://aka.ms/midi") << std::endl;
        std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
        std::cout << dye::light_aqua(" If this is a Windows Insider Dev or Beta build of Windows released before our official") << std::endl;
        std::cout << dye::light_aqua(" 24h2+ retail release, you may receive an incorrect positive result from this test. ") << std::endl;
        std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
        std::cout << std::endl;
    }

    try
    {

        WriteHeading("Looking for wdmaud2.drv in registry");

        bool wdmaud2Enabled = VerifyWdmaud2Registry();

        if (wdmaud2Enabled)
        {
            WriteInfo("Successfully verified wdmaud2.drv is present in registry.");
        }
        else
        {
            WriteInfo("wdmaud2.drv is not present in registry. Most likely, the feature has not yet been enabled on this PC.");
            return static_cast<int>(MIDISRV_CHECK_RETURN_VALUE_NOT_ENABLED_IN_REGISTRY);
        }

        // activation checks
        WriteHeading("Testing MIDI Service Activation");

        bool serviceAvailable = VerifyMidiSrvConnectivity();

        if (serviceAvailable)
        {
            if (ValidateInBoxMidisrvPath())
            {
                WriteInfo("Successfully tested connectivity to service: MIDI Service is available and running from System32.");

                return static_cast<int>(MIDISRV_CHECK_RETURN_VALUE_SUCCESS);
            }
            else
            {
                WriteInfo("Successfully tested connectivity to service: MIDI Service is available.");
                WriteInfo("However, this appears to be a development build, and so may not have the right");
                WriteInfo("connections to the MIDI 1.0 APIs, the MIDI 2.0 class driver, etc.");

                return static_cast<int>(MIDISRV_CHECK_RETURN_VALUE_SUCCESS_DEV_BUILD);
            }
        }
        else
        {
            WriteError("MIDI Service is not available. It may not yet be enabled on this PC via Controlled Feature Rollout.");

            // we only shut down midisrv if we're not running in quiet mode. The reason is the shutdown
            // will throw up a UAC prompt
            if (!m_optionQuiet)
            {
                ShutdownMidisrv();
            }

            return static_cast<int>(MIDISRV_CHECK_RETURN_VALUE_NOT_ENABLED_OR_NOT_STARTED);
        }


    }
    catch (winrt::hresult_error error)
    {
        WriteError("Unexpected error attempting to communicate with service through RPC call.");

        ShutdownMidisrv();

        // service transport is not available.
        return static_cast<int>(MIDISRV_CHECK_RETURN_VALUE_NOT_INSTALLED);
    }
}

