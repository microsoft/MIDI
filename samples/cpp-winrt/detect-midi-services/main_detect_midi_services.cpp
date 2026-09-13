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
// There are two questions, and you have to ask both. They fail for different
// reasons and the answer to the first tells you nothing about the second.
//
//   1. Is the API present at all? Windows.Devices.Midi2.MidiApi either resolves
//      on this machine or it does not. A PC which predates Windows MIDI
//      Services has no registration for the class.
//
//   2. Is it usable? A PC can have the API and still be configured to use the
//      old MIDI stack, because the customer chose Legacy API mode. MidiApi's
//      EnsureServiceAvailable() answers this, and also demand-starts the
//      service, which is why you call it before you enumerate anything.
//      https://microsoft.github.io/MIDI/kb/how-to-change-api-mode/
//
// Both ways of asking question 1 are shown here:
//
//   * Using the projection, which is what you want if you already reference the
//     Windows MIDI Services NuGet package (or, in box, the Windows SDK).
//
//   * Using RoGetActivationFactory with the class name and the published IID,
//     which needs no package reference, no .winmd and no import library. Use
//     this if you do not want a build-time dependency on the SDK at all.
//
// Note that Windows.Devices.Midi2.MidiApi is a *static* runtime class. It has
// no instances, so RoActivateInstance will not work on it. You ask for its
// activation factory and call the statics interface on that.

#include <iostream>

#include <Windows.h>
#include <mmeapi.h>

#pragma comment(lib, "winmm.lib")

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
// Question 1, option A: using the projection
// ---------------------------------------------------------------------------

// Asking for the activation factory is the whole test. It does not touch the
// service, so it is cheap and safe to call before you have decided anything.
//
// try_get_activation_factory is the non-throwing form. If you would rather write
// the short version, midi2::MidiApi::EnsureServiceAvailable() answers both
// questions at once, but it throws winrt::hresult_error when the class is not
// registered, so it has to be inside a try/catch. Do not let that exception
// escape into a host application, especially not from a plug-in.
bool TryGetMidiApiStatics(midi2::IMidiApiStatics& statics)
{
    winrt::hresult_error error{};

    statics = winrt::try_get_activation_factory<midi2::MidiApi, midi2::IMidiApiStatics>(error);

    if (!statics)
    {
        std::wcout
            << L"  Windows.Devices.Midi2.MidiApi did not resolve. HRESULT 0x"
            << std::hex << static_cast<uint32_t>(error.code()) << std::dec
            << L". This PC does not have Windows MIDI Services." << std::endl;

        return false;
    }

    return true;
}


// ---------------------------------------------------------------------------
// Questions 1 and 2, option B: without any reference to the SDK
// ---------------------------------------------------------------------------

namespace abi
{
    // Declared by hand so this file would still compile with the NuGet package
    // and the .winmd removed. The IID is published in the SDK reference
    // documentation and is stable for the life of the API. Method order is the
    // vtable order and must match the documentation exactly.
    struct __declspec(uuid("8087b303-0519-c0de-31d1-ee0010000000")) IMidiApiStatics : ::IInspectable
    {
        virtual HRESULT __stdcall EnsureServiceAvailable(::boolean* result) = 0;
        virtual HRESULT __stdcall GetCurrentlySelectedApiMode(int32_t* result) = 0;
    };
}

// com_ptr here comes from C++/WinRT itself, not from the MIDI package. Any COM
// smart pointer, or a raw pointer you release yourself, works the same way.

// RoGetActivationFactory resolves a class through its system registration, which
// is what you get once the SDK ships in Windows. During the developer preview
// the SDK is deployed next to your application instead of being registered, so
// there is nothing for RoGetActivationFactory to find and it returns
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
    std::wcout << L"The same two questions, with no SDK reference:" << std::endl;

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
            << L". This PC does not have Windows MIDI Services." << std::endl;

        return;
    }

    ::boolean serviceAvailable{};
    int32_t mode{};

    if (SUCCEEDED(statics->EnsureServiceAvailable(&serviceAvailable)) &&
        SUCCEEDED(statics->GetCurrentlySelectedApiMode(&mode)))
    {
        std::wcout
            << L"  API present. Service available: " << (serviceAvailable ? L"yes" : L"no")
            << L". " << ApiModeName(static_cast<midi2::MidiApiMode>(mode)) << L"." << std::endl;
    }
}


// ---------------------------------------------------------------------------
// The decision tree
// ---------------------------------------------------------------------------

MidiBackend ChooseBackend()
{
    std::wcout << L"Question 1: is the Windows MIDI Services API present?" << std::endl;

    midi2::IMidiApiStatics statics{ nullptr };

    if (!TryGetMidiApiStatics(statics))
    {
        return MidiBackend::LegacyMidi1Api;
    }

    std::wcout << L"  Yes." << std::endl << std::endl;

    std::wcout << L"Question 2: is it usable on this PC right now?" << std::endl;

    // Demand-starts the service. False means the API is installed but this PC is
    // not set up to use it, or the service could not be started.
    if (!statics.EnsureServiceAvailable())
    {
        auto const mode = statics.GetCurrentlySelectedApiMode();

        std::wcout << L"  No. The PC is in " << ApiModeName(mode) << L"." << std::endl;

        if (mode == midi2::MidiApiMode::LegacyMode)
        {
            // A deliberate customer choice, not a failure. Say so, and carry on
            // with the old APIs rather than reporting an error they cannot act on.
            std::wcout
                << L"  This is a supported configuration which the customer selected."
                << std::endl;
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

    auto const backend = ChooseBackend();

    std::wcout << std::endl;

    // The same two questions, asked with no reference to the SDK at all. Shown
    // here for comparison. Your own code would use one approach or the other.
    ReportDetectionWithoutProjection();

    std::wcout << std::endl;

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

