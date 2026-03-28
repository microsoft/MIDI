// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

// Full Windows MIDI Services detection sample
//
// This sample demonstrates how to check whether Windows MIDI Services is
// fully installed, enabled, and functional on a PC. It performs four checks:
//
//   1. Registry check   - is wdmaud2.drv registered? (Controlled Feature Rollout)
//   2. COM check        - are the MIDI Service COM interfaces present?
//   3. Service check    - is midisrv.exe registered and not disabled?
//   4. SDK check        - is the SDK Runtime installed? (optional)
//
// Unlike the simpler detectmidi sample, this handles the case where Windows
// MIDI Services bits are present but disabled via Controlled Feature Rollout.
//
// See also: midicheckservice tool in src/app-sdk/tools/ for the production
// version of this logic, which includes RPC connectivity verification.
//
// Return codes:
//   0  = All checks passed, Windows MIDI Services is available
//  -1  = COM initialization failed
//   1  = Feature not enabled in registry (CFR not rolled out to this PC)
//   2  = COM interfaces not registered (bits not installed)
//   3  = Service not available (access denied, disabled, missing, or not startable)

#include "pch.h"


// ============================================================================
// MIDI Service COM interface GUID
// This is the MidiSrvTransport placeholder used to detect if the MIDI Service
// bits are installed on the system. This UUID should not change.
// ============================================================================

struct __declspec(uuid("2BA15E4E-5417-4A66-85B8-2B2260EFBC84")) MidiSrvTransportPlaceholder : ::IUnknown
{ };


// ============================================================================
// MIDI SDK Client Initializer COM interface GUID
// Used to detect if the SDK Runtime and Tools are installed.
// ============================================================================

struct __declspec(uuid("c3263827-c3b0-bdbd-2500-ce63a3f3f2c3")) MidiClientInitializer : ::IUnknown
{ };


// ============================================================================
// Check 1: Registry - Is wdmaud2.drv present?
//
// Windows MIDI Services uses Controlled Feature Rollout (CFR) to gradually
// enable the feature on PCs. Even after a Windows Update installs the bits,
// the feature may not be enabled yet. The presence of wdmaud2.drv in the
// Drivers32 registry key is the indicator that CFR has enabled it.
// ============================================================================

bool CheckWdmaud2Registry()
{
    std::wstring keyPath = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Drivers32";

    for (int i = 0; i < 10; i++)
    {
        std::wstring valueName = (i == 0) ? L"midi" : L"midi" + std::to_wstring(i);

        auto val = wil::reg::try_get_value_string(HKEY_LOCAL_MACHINE, keyPath.c_str(), valueName.c_str());

        if (val.has_value() && val.value() == L"wdmaud2.drv")
        {
            return true;
        }
    }

    return false;
}


// ============================================================================
// Check 2: COM - Are MIDI Service interfaces registered?
//
// This verifies that the MIDI Service COM class is registered. It loads the
// COM server DLL into the process to test activation. Note: this alone is not
// sufficient because CFR may have the bits present but disabled.
// ============================================================================

bool CheckMidiServiceComRegistration()
{
    wil::com_ptr_nothrow<IUnknown> servicePointer;

    HRESULT hr = CoCreateInstance(
        __uuidof(MidiSrvTransportPlaceholder),
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&servicePointer)
    );

    return SUCCEEDED(hr);
}


// ============================================================================
// Check 3: Service - Is midisrv.exe registered and not disabled?
//
// Verify that the MIDI Service exists in the Service Control Manager and is
// not disabled. A disabled service means CFR has not enabled it or it was
// manually disabled. This does NOT verify RPC connectivity — for that, see
// the midicheckservice tool which calls MidiSrvVerifyConnectivity().
// ============================================================================

// Returns:
//   0 = service is available and not disabled
//   1 = access denied opening Service Control Manager
//   2 = service not found (midisrv not registered)
//   3 = access denied opening midisrv service
//   4 = service is disabled
//  -1 = other error (config query failed, allocation failed)
int CheckMidiServiceAvailable()
{
    SC_HANDLE hSCManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCManager)
    {
        return (GetLastError() == ERROR_ACCESS_DENIED) ? 1 : -1;
    }

    auto closeSCM = wil::scope_exit([&] { CloseServiceHandle(hSCManager); });

    SC_HANDLE hService = OpenServiceW(hSCManager, L"midisrv", SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (!hService)
    {
        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED) return 3;
        if (err == ERROR_SERVICE_DOES_NOT_EXIST) return 2;
        return -1;
    }

    auto closeSvc = wil::scope_exit([&] { CloseServiceHandle(hService); });

    // Query service configuration to check if it is disabled
    DWORD bytesNeeded = 0;
    QueryServiceConfigW(hService, nullptr, 0, &bytesNeeded);

    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || bytesNeeded == 0)
    {
        return -1;
    }

    LPQUERY_SERVICE_CONFIGW config = static_cast<LPQUERY_SERVICE_CONFIGW>(LocalAlloc(LPTR, bytesNeeded));
    if (!config)
    {
        return -1;
    }

    auto freeConfig = wil::scope_exit([&] { LocalFree(config); });

    if (!QueryServiceConfigW(hService, config, bytesNeeded, &bytesNeeded))
    {
        return -1;
    }

    // A disabled service cannot be started — this is the key check for CFR
    if (config->dwStartType == SERVICE_DISABLED)
    {
        return 4;
    }

    return 0;
}


// ============================================================================
// Check 4: SDK Runtime - Is the SDK Runtime installed?
//
// This is optional. The SDK Runtime is only needed if your application uses
// the Windows MIDI Services SDK (the NuGet package). Applications that use
// only WinMM or WinRT MIDI 1.0 APIs do not need the SDK Runtime.
// ============================================================================

bool CheckSdkRuntimeInstalled()
{
    wil::com_ptr_nothrow<IUnknown> initPointer;

    HRESULT hr = CoCreateInstance(
        __uuidof(MidiClientInitializer),
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&initPointer)
    );

    return SUCCEEDED(hr);
}


// ============================================================================
// Main
// ============================================================================

int __cdecl main()
{
    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hrInit))
    {
        std::cout << "Failed to initialize COM." << std::endl;
        return -1;
    }

    auto comCleanup = wil::scope_exit([] { CoUninitialize(); });

    std::cout << "Windows MIDI Services - Full Detection Sample" << std::endl;
    std::cout << "==============================================" << std::endl;
    std::cout << std::endl;

    // ---- Check 1: Registry (CFR) ----
    std::cout << "[1/4] Checking registry for wdmaud2.drv (Controlled Feature Rollout)... ";

    bool registryOk = CheckWdmaud2Registry();
    std::cout << (registryOk ? "FOUND" : "NOT FOUND") << std::endl;

    if (!registryOk)
    {
        std::cout << std::endl;
        std::cout << "RESULT: Windows MIDI Services has not been enabled on this PC." << std::endl;
        std::cout << "The feature is rolled out gradually via Windows Update." << std::endl;
        return 1;
    }

    // ---- Check 2: COM registration ----
    std::cout << "[2/4] Checking MIDI Service COM registration... ";

    bool comOk = CheckMidiServiceComRegistration();
    std::cout << (comOk ? "REGISTERED" : "NOT REGISTERED") << std::endl;

    if (!comOk)
    {
        std::cout << std::endl;
        std::cout << "RESULT: MIDI Service COM interfaces are not registered." << std::endl;
        std::cout << "The MIDI Service bits may not be installed on this version of Windows." << std::endl;
        return 2;
    }

    // ---- Check 3: Service availability ----
    std::cout << "[3/4] Checking MIDI Service (midisrv.exe) availability... ";

    int serviceResult = CheckMidiServiceAvailable();
    std::cout << (serviceResult == 0 ? "AVAILABLE" : "NOT AVAILABLE") << std::endl;

    if (serviceResult != 0)
    {
        std::cout << std::endl;
        if (serviceResult == 1 || serviceResult == 3)
        {
            std::cout << "RESULT: Access denied querying the MIDI Service." << std::endl;
            std::cout << "Try running this tool as Administrator." << std::endl;
        }
        else
        {
            std::cout << "RESULT: MIDI Service is not available." << std::endl;
            std::cout << "The service may be disabled or not yet enabled via Controlled Feature Rollout." << std::endl;
            std::cout << "Try running 'midifixreg' from an Administrator command prompt and reboot." << std::endl;
        }
        return 3;
    }

    // ---- Check 4: SDK Runtime (optional) ----
    std::cout << "[4/4] Checking SDK Runtime installation (optional)... ";

    bool sdkOk = CheckSdkRuntimeInstalled();
    std::cout << (sdkOk ? "INSTALLED" : "NOT INSTALLED") << std::endl;

    std::cout << std::endl;
    std::cout << "RESULT: Windows MIDI Services is installed and enabled." << std::endl;

    if (!sdkOk)
    {
        std::cout << "Note: The SDK Runtime is not installed. If your app uses the MIDI SDK," << std::endl;
        std::cout << "download it from https://aka.ms/midi" << std::endl;
    }

    return 0;
}
