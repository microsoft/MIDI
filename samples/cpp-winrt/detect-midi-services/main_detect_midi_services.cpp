// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: deciding, at runtime, whether to drive Windows MIDI
// Services or to fall back to WinMM / WinRT MIDI 1.0.
//
// This matters most to library and framework authors, who ship one binary that
// has to run on a Windows 11 PC with Windows MIDI Services, on an older
// Windows, and on a PC where the customer has deliberately switched the machine
// back to the old MIDI stack.
//
// There are three questions, and you have to ask all of them, in this order.
// They fail for different reasons, and no answer tells you anything about the
// next one.
//
//   1. Is this a version of Windows the API is supported on? The Windows MIDI
//      Services API needs Windows 11 25H2 or later. Windows 11 24H2 has an
//      older Windows MIDI Services inside it, but it never gets the API or any
//      more fixes, so don't use the API there even if a copy of it loads. This
//      is the one place you have to check the Windows version: 24H2 and 25H2
//      run the same code, so nothing else tells them apart.
//
//   2. Is the API present? Windows.Devices.Midi2.MidiApi either resolves on
//      this machine or it does not. It resolves from the copy that comes with
//      Windows, or from a copy your app ships next to itself, and
//      MidiApi.IsProvidedByWindows() tells you which one you got. When Windows
//      has its own copy, that is always the one you get.
//
//   3. Is it usable? A PC can have the API and still be set to use the old
//      MIDI stack, because the customer chose Legacy API mode, and the MIDI
//      service can be turned off. MidiApi's EnsureServiceAvailable() answers
//      this, and also demand-starts the service, which is why you call it
//      before you enumerate anything.
//      https://microsoft.github.io/MIDI/kb/how-to-change-api-mode/
//
// Both ways of asking questions 2 and 3 are shown here:
//
//   * Using the projection, which is what you want if you already reference the
//     Windows MIDI Services NuGet package (or, in box, the Windows SDK).
//
//   * Using RoGetActivationFactory with the class name and the published IIDs,
//     which needs no package reference, no .winmd and no import library. Use
//     this if you do not want a build-time dependency on the SDK at all.
//
// Note that Windows.Devices.Midi2.MidiApi is a *static* runtime class. It has
// no instances, so RoActivateInstance will not work on it. You ask for its
// activation factory and call the statics interfaces on that.

#include <iostream>
#include <memory>
#include <vector>

#include <Windows.h>
#include <mmeapi.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "advapi32.lib")

#include <inspectable.h>
#include <activation.h>
#include <roapi.h>
#include <winstring.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Midi.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>

namespace midi2 = winrt::Windows::Devices::Midi2;
namespace midi2enum = winrt::Windows::Devices::Midi2::Enumeration;
namespace oldmidi = winrt::Windows::Devices::Midi;              // the WinRT MIDI 1.0 API from Windows 10
namespace enumeration = winrt::Windows::Devices::Enumeration;
namespace collections = winrt::Windows::Foundation::Collections;


// The only thing the rest of your application should branch on. Work it out once
// at startup and keep it. The API mode cannot change without a reboot.
enum class MidiBackend
{
    WindowsMidiServices,
    LegacyMidi1Api
};

std::wstring_view ApiModeName(midi2::MidiApiMode const mode)
{
    switch (mode)
    {
    case midi2::MidiApiMode::FullWindowsMidiServicesMode:   return L"Full Windows MIDI Services mode";
    case midi2::MidiApiMode::LegacyMode:                    return L"Legacy API mode";
    case midi2::MidiApiMode::HybridLegacyMode:              return L"Hybrid Legacy API mode";
    default:                                                return L"Unrecognized mode";
    }
}


// ---------------------------------------------------------------------------
// Question 1: is this a version of Windows the API is supported on?
// ---------------------------------------------------------------------------

// 24H2 and 25H2 run the same code, so the build number is the only thing that
// tells them apart.
constexpr DWORD Windows11Version24H2Build{ 26100 };
constexpr DWORD Windows11Version25H2Build{ 26200 };

// GetVersionEx and the version helper functions report Windows 8 to a process
// that has no Windows 10 manifest, and a library or plug-in can't control its
// host's manifest. RtlGetVersion reports the real build number.
DWORD GetWindowsBuildNumber()
{
    using RtlGetVersionProc = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);

    auto const rtlGetVersion = reinterpret_cast<RtlGetVersionProc>(
        ::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));

    RTL_OSVERSIONINFOW info{ sizeof(info) };

    if (rtlGetVersion == nullptr || rtlGetVersion(&info) != 0)
    {
        return 0;
    }

    return info.dwBuildNumber;
}

// The sample doesn't use the API on 24H2, so it can't ask EnsureServiceAvailable().
// Instead it reads the two things the API mode article tells customers about:
// the UseLegacyMidi setting and the midisrv service.
bool IsWindowsMidiServicesInUse()
{
    DWORD apiMode{};
    DWORD size{ sizeof(apiMode) };

    if (::RegGetValueW(
            HKEY_LOCAL_MACHINE,
            LR"(SOFTWARE\Microsoft\Windows NT\CurrentVersion\Drivers32)",
            L"UseLegacyMidi",
            RRF_RT_REG_DWORD,
            nullptr,
            &apiMode,
            &size) == ERROR_SUCCESS && apiMode == 1)
    {
        return false;
    }

    using unique_service_handle = std::unique_ptr<std::remove_pointer_t<SC_HANDLE>, decltype(&::CloseServiceHandle)>;

    unique_service_handle const manager{ ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT), &::CloseServiceHandle };

    if (!manager)
    {
        return false;
    }

    unique_service_handle const service{ ::OpenServiceW(manager.get(), L"midisrv", SERVICE_QUERY_CONFIG), &::CloseServiceHandle };

    if (!service)
    {
        return false;
    }

    DWORD bytesNeeded{};
    ::QueryServiceConfigW(service.get(), nullptr, 0, &bytesNeeded);

    std::vector<BYTE> buffer(bytesNeeded);
    auto const config = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data());

    if (bytesNeeded == 0 || !::QueryServiceConfigW(service.get(), config, bytesNeeded, &bytesNeeded))
    {
        return false;
    }

    return config->dwStartType != SERVICE_DISABLED;
}


// ---------------------------------------------------------------------------
// Questions 2 and 3, option A: using the projection
// ---------------------------------------------------------------------------

// Asking for the activation factory is the whole test for question 2. It does
// not touch the service, so it is cheap and safe to call before you have
// decided anything.
//
// try_get_activation_factory is the non-throwing form. If you would rather write
// the short version, midi2::MidiApi::EnsureServiceAvailable() answers questions
// 2 and 3 at once, but it throws winrt::hresult_error when the class is not
// registered, so it has to be inside a try/catch. Do not let that exception
// escape into a host application, especially not from a plug-in.
bool TryGetMidiApiStatics(midi2::IMidiApiStatics& statics)
{
    winrt::hresult_error error{};

    statics = winrt::try_get_activation_factory<midi2::MidiApi, midi2::IMidiApiStatics>(error);

    if (!statics)
    {
        std::wcout
            << L"  No. Windows.Devices.Midi2.MidiApi did not resolve. HRESULT 0x"
            << std::hex << static_cast<uint32_t>(error.code()) << std::dec << L"." << std::endl
            << L"  This version of Windows doesn't include the Windows MIDI Services API yet. It comes in a Windows update." << std::endl;

        return false;
    }

    return true;
}

// midi2::MidiApi::IsProvidedByWindows() is the short form, but it throws when the
// copy that loaded is too old to have it. Every copy that comes with Windows has
// it, so a copy without it can only be one that came with an app.
bool IsProvidedByWindows(midi2::IMidiApiStatics const& statics)
{
    auto const statics2 = statics.try_as<midi2::IMidiApiStatics2>();

    return statics2 && statics2.IsProvidedByWindows();
}


// ---------------------------------------------------------------------------
// Questions 2 and 3, option B: without any reference to the SDK
// ---------------------------------------------------------------------------

namespace abi
{
    // Declared by hand so this file would still compile with the NuGet package
    // and the .winmd removed. The IIDs come from the API's metadata and are
    // stable for the life of the API. Method order is the vtable order and must
    // match the metadata exactly.
    struct __declspec(uuid("8087b303-0519-c0de-31d1-ee0010000000")) IMidiApiStatics : ::IInspectable
    {
        virtual HRESULT __stdcall EnsureServiceAvailable(::boolean* result) = 0;
        virtual HRESULT __stdcall GetCurrentlySelectedApiMode(int32_t* result) = 0;
    };

    struct __declspec(uuid("8087b303-0519-c0de-31d1-ee0010000002")) IMidiApiStatics2 : ::IInspectable
    {
        virtual HRESULT __stdcall IsProvidedByWindows(::boolean* result) = 0;
    };
}

// com_ptr here comes from C++/WinRT itself, not from the MIDI package. Any COM
// smart pointer, or a raw pointer you release yourself, works the same way.

// RoGetActivationFactory resolves a class through its system registration, which
// is what you get once Windows has its own copy of the API. Until then, the only
// copy is the one deployed next to your application, which is not registered,
// so there is nothing for RoGetActivationFactory to find and it returns
// REGDB_E_CLASSNOTREG. Asking the module directly covers that case. C++/WinRT
// does the same thing internally, which is why the projected call above can
// succeed on a machine where RoGetActivationFactory alone fails.
HRESULT GetStaticsFromModule(HSTRING const name, winrt::com_ptr<abi::IMidiApiStatics>& statics)
{
    // Constrained search path. Never let a bare DLL name be resolved from the
    // current directory or anywhere else an attacker can write.
    auto const module = ::LoadLibraryExW(
        L"Windows.Devices.Midi2.dll",
        nullptr,
        LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_APPLICATION_DIR);

    if (module == nullptr)
    {
        return HRESULT_FROM_WIN32(::GetLastError());
    }

    using DllGetActivationFactoryProc = HRESULT(__stdcall*)(HSTRING, ::IActivationFactory**);

    auto const getActivationFactory = reinterpret_cast<DllGetActivationFactoryProc>(
        ::GetProcAddress(module, "DllGetActivationFactory"));

    if (getActivationFactory == nullptr)
    {
        return HRESULT_FROM_WIN32(::GetLastError());
    }

    winrt::com_ptr<::IActivationFactory> factory;

    if (auto const hr = getActivationFactory(name, factory.put()); FAILED(hr))
    {
        return hr;
    }

    return factory->QueryInterface(__uuidof(abi::IMidiApiStatics), statics.put_void());
}

void ReportDetectionWithoutProjection()
{
    std::wcout << L"Questions 2 and 3 again, with no SDK reference:" << std::endl;

    constexpr wchar_t className[]{ L"Windows.Devices.Midi2.MidiApi" };

    HSTRING_HEADER header{};
    HSTRING name{};

    if (FAILED(WindowsCreateStringReference(className, ARRAYSIZE(className) - 1, &header, &name)))
    {
        return;
    }

    winrt::com_ptr<abi::IMidiApiStatics> statics;

    auto hr = RoGetActivationFactory(name, __uuidof(abi::IMidiApiStatics), statics.put_void());

    if (FAILED(hr))
    {
        hr = GetStaticsFromModule(name, statics);
    }

    if (FAILED(hr))
    {
        std::wcout
            << L"  MidiApi did not resolve. HRESULT 0x"
            << std::hex << static_cast<uint32_t>(hr) << std::dec
            << L". This version of Windows doesn't include the API yet." << std::endl;

        return;
    }

    ::boolean providedByWindows{};
    winrt::com_ptr<abi::IMidiApiStatics2> statics2;

    // Left false for a copy too old to have IMidiApiStatics2, which can only be one that came with an app.
    if (SUCCEEDED(statics->QueryInterface(__uuidof(abi::IMidiApiStatics2), statics2.put_void())))
    {
        if (FAILED(statics2->IsProvidedByWindows(&providedByWindows)))
        {
            providedByWindows = false;
        }
    }

    ::boolean serviceAvailable{};
    int32_t mode{};

    if (SUCCEEDED(statics->EnsureServiceAvailable(&serviceAvailable)) &&
        SUCCEEDED(statics->GetCurrentlySelectedApiMode(&mode)))
    {
        std::wcout
            << L"  API present: " << (providedByWindows ? L"the copy that comes with Windows" : L"the app's own copy")
            << L". Service available: " << (serviceAvailable ? L"yes" : L"no")
            << L". " << ApiModeName(static_cast<midi2::MidiApiMode>(mode)) << L"." << std::endl;
    }
}


// ---------------------------------------------------------------------------
// The decision tree
// ---------------------------------------------------------------------------

MidiBackend ChooseBackend(DWORD const windowsBuild)
{
    std::wcout << L"Question 1: is this a version of Windows the API is supported on?" << std::endl;

    if (windowsBuild < Windows11Version24H2Build)
    {
        std::wcout
            << L"  No. This PC is build " << windowsBuild
            << L". The Windows MIDI Services API needs Windows 11 25H2 or later." << std::endl;

        return MidiBackend::LegacyMidi1Api;
    }

    if (windowsBuild < Windows11Version25H2Build)
    {
        // Don't use the API here, even if a copy of it would load.
        std::wcout << L"  No. This is Windows 11 24H2, which doesn't get the Windows MIDI Services API." << std::endl;

        if (IsWindowsMidiServicesInUse())
        {
            std::wcout
                << L"  Windows MIDI Services on Windows 11 24H2 is an older version that doesn't get fixes." << std::endl
                << L"  Update to Windows 11 25H2 or later. If you can't update, we recommend Legacy API mode." << std::endl
                << L"  https://microsoft.github.io/MIDI/kb/how-to-change-api-mode/" << std::endl;
        }

        return MidiBackend::LegacyMidi1Api;
    }

    std::wcout << L"  Yes. This PC is build " << windowsBuild << L"." << std::endl << std::endl;

    std::wcout << L"Question 2: is the Windows MIDI Services API present?" << std::endl;

    midi2::IMidiApiStatics statics{ nullptr };

    if (!TryGetMidiApiStatics(statics))
    {
        return MidiBackend::LegacyMidi1Api;
    }

    if (IsProvidedByWindows(statics))
    {
        std::wcout << L"  Yes. It's the copy that comes with Windows." << std::endl << std::endl;
    }
    else
    {
        std::wcout << L"  Yes. It's the app's own copy, not the one that comes with Windows." << std::endl << std::endl;
    }

    std::wcout << L"Question 3: is it usable on this PC right now?" << std::endl;

    // Demand-starts the service. False means the PC is in Legacy API mode, or the
    // MIDI service is turned off or could not be started.
    if (!statics.EnsureServiceAvailable())
    {
        auto const mode = statics.GetCurrentlySelectedApiMode();

        if (mode == midi2::MidiApiMode::LegacyMode)
        {
            // A deliberate customer choice, not a failure. Say so, and carry on
            // with the old APIs rather than reporting an error they cannot act on.
            std::wcout
                << L"  No. The PC is in " << ApiModeName(mode) << L"." << std::endl
                << L"  This is a supported configuration which the customer selected." << std::endl;
        }
        else
        {
            std::wcout << L"  No. The MIDI service is turned off or couldn't start." << std::endl;
        }

        return MidiBackend::LegacyMidi1Api;
    }

    auto const mode = statics.GetCurrentlySelectedApiMode();

    std::wcout << L"  Yes. The PC is in " << ApiModeName(mode) << L"." << std::endl;

    if (mode == midi2::MidiApiMode::HybridLegacyMode)
    {
        // Hybrid is the one mode where neither answer is complete: devices on
        // MIDI 1.0 drivers are reachable only from WinMM and WinRT MIDI 1.0, and
        // devices on the new class driver are reachable only from here.
        std::wcout
            << L"  Devices using MIDI 1.0 drivers will not appear here in this mode."
            << std::endl;
    }

    return MidiBackend::WindowsMidiServices;
}


// ---------------------------------------------------------------------------
// Branch 1: Windows MIDI Services
// ---------------------------------------------------------------------------

void UseWindowsMidiServices()
{
    std::wcout << L"Using Windows MIDI Services." << std::endl << std::endl;

    collections::IVectorView<midi2enum::MidiEndpointDeviceInformation> endpoints =
        midi2enum::MidiEndpointDeviceInformation::FindAll(
            midi2enum::MidiEndpointDeviceInformationSortOrder::Name,
            midi2enum::MidiEndpointDeviceInformationFilters::AllStandardEndpoints);

    std::wcout << endpoints.Size() << L" UMP endpoint(s):" << std::endl;

    for (auto const& endpoint : endpoints)
    {
        std::wcout << L"  " << endpoint.Name().c_str() << std::endl;
    }
}


// ---------------------------------------------------------------------------
// Branch 2: the older MIDI 1.0 APIs
// ---------------------------------------------------------------------------

void UseWinMM()
{
    std::wcout << L"WinMM:" << std::endl;

    auto const inputCount = midiInGetNumDevs();
    auto const outputCount = midiOutGetNumDevs();

    std::wcout << L"  " << inputCount << L" input port(s), " << outputCount << L" output port(s)" << std::endl;

    for (UINT i = 0; i < outputCount; i++)
    {
        MIDIOUTCAPSW caps{};

        if (midiOutGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
        {
            std::wcout << L"  out " << i << L": " << caps.szPname << std::endl;
        }
    }
}

void UseWinRTMidi1()
{
    std::wcout << L"WinRT MIDI 1.0:" << std::endl;

    // Blocking on the async call is fine in a console app running in a
    // multi-threaded apartment. Do not do this on a UI thread.
    auto const outputs = enumeration::DeviceInformation::FindAllAsync(
        oldmidi::MidiOutPort::GetDeviceSelector()).get();

    std::wcout << L"  " << outputs.Size() << L" output port(s)" << std::endl;

    for (auto const& port : outputs)
    {
        std::wcout << L"  " << port.Name().c_str() << std::endl;
    }
}

void UseLegacyMidi1Api()
{
    // Both of these are still fully supported. Pick whichever your codebase
    // already uses; there is no benefit to moving between them.
    std::wcout << L"Falling back to the MIDI 1.0 APIs." << std::endl << std::endl;

    UseWinMM();
    std::wcout << std::endl;
    UseWinRTMidi1();
}


int main()
{
    winrt::init_apartment();

    std::wcout << L"Windows MIDI Services detection" << std::endl << std::endl;

    auto const windowsBuild = GetWindowsBuildNumber();

    auto const backend = ChooseBackend(windowsBuild);

    std::wcout << std::endl;

    // Questions 2 and 3 again, with no reference to the SDK at all. Shown here
    // for comparison. Your own code would use one approach or the other.
    if (windowsBuild >= Windows11Version25H2Build)
    {
        ReportDetectionWithoutProjection();

        std::wcout << std::endl;
    }

    switch (backend)
    {
    case MidiBackend::WindowsMidiServices:
        UseWindowsMidiServices();
        break;

    case MidiBackend::LegacyMidi1Api:
        UseLegacyMidi1Api();
        break;
    }

    return 0;
}

