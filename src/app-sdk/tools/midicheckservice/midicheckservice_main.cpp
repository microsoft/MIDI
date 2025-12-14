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


#include "MidiSrvRpc_c.c"   // not for use directly by applications. The RPC calls are an internal implementation detail and may change at any point

bool m_optionQuiet{ false };


void WriteHeading(std::string info)
{
    if (m_optionQuiet) return;

    std::cout << std::endl << dye::aqua(info) << std::endl;
}

void WriteInfo(std::string info)
{
    if (m_optionQuiet) return;

    std::cout << " - " << dye::white(info) << std::endl;
}

void WriteError(std::string info)
{
    if (m_optionQuiet) return;

    std::cout << " - " << dye::red(info) << std::endl;
}

#define LINE_LENGTH 96


// return codes from this app
#define MIDISRV_CHECK_RETURN_VALUE_SUCCESS                      0   // service has been installed and enabled
#define MIDISRV_CHECK_RETURN_VALUE_SUCCESS_DEV_BUILD           -1   // service works but is a developer build, not the in-box build

#define MIDISRV_CHECK_RETURN_VALUE_NOT_INSTALLED                1   // service has not been installed, because the COM interfaces are not present
#define MIDISRV_CHECK_RETURN_VALUE_NOT_ENABLED_OR_NOT_STARTED   2   // COM interface available, but service cannot be started. This may be due to Controlled Feature Rollout, or a system problem.

#define MIDISRV_CHECK_RETURN_VALUE_CHECK_SKIPPED                99  // An option was passed which skipped evaluation. For example, --help



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
bool ParseCommandLine(int argc, WCHAR* argv[])
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

// this RPC code is used by this tool, but is not supported for use by applications 
// directly. It could change in the future, as an internal implementation detail of 
// Windows MIDI Services.

_Use_decl_annotations_
void __RPC_FAR* __RPC_USER midl_user_allocate(size_t byteCount
)
{
    return (void*)new (std::nothrow) BYTE[byteCount];
}

_Use_decl_annotations_
void __RPC_USER midl_user_free(void __RPC_FAR* pointer
)
{
    delete[](BYTE*)pointer;
}

bool VerifyMidiSrvConnectivity()
{
    wil::unique_rpc_binding bindingHandle;
    bindingHandle = NULL;

    RPC_WSTR stringBinding = nullptr;
    auto cleanupOnExit = wil::scope_exit([&]()
        {
            if (stringBinding)
            {
                RpcStringFree(&stringBinding);
            }
        });

    RpcStringBindingCompose(
        nullptr,
        (RPC_WSTR)MIDISRV_LRPC_PROTOCOL,
        nullptr,
        nullptr,
        nullptr,
        &stringBinding);

    RpcBindingFromStringBinding(
        stringBinding,
        &bindingHandle);

    bool result = false;

    auto verifyConnectivity = ([&]()
        {
            // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
            // with structured exception handling.
            RpcTryExcept result = MidiSrvVerifyConnectivity(bindingHandle.get());
            RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
            RpcEndExcept

            return S_OK;
        });

    auto hr = verifyConnectivity();


    if (FAILED(hr))
    {
        return false;
    }

    return result;
}

// End RPC code ---------------



void ShutdownMidisrv()
{
    WriteInfo("Shutting down the MIDI service, if present. You may receive an elevation prompt for this action.");

    if (!m_optionQuiet)
    {
        system("pause");
    }

    ShellExecute(
        NULL,
        L"runas",
        L"net",
        L"stop midisrv",
        NULL,
        SW_HIDE
    );

}

bool GetMidisrvPathName(std::wstring& pathName)
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

int __cdecl wmain(int argc, WCHAR* argv[])
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
        std::cout << std::endl;
    }

    // activation checks
    try
    {
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

            ShutdownMidisrv();

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

