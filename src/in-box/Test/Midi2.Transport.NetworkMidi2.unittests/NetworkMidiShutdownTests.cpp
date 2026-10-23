// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    // Midi2NetworkMidiTransport in Midi2NetworkMidiTransport.idl
    constexpr GUID TransportClsid{ 0xc95dcd1f, 0xcde3, 0x4c2d, { 0x91, 0x3c, 0x52, 0x8c, 0xb8, 0xa4, 0xcb, 0xe6 } };

    // Counts references and nothing else. Every stand-in outlives the transport, so it never
    // deletes itself.
    template <typename Interface>
    class CountedStandIn : public Interface
    {
    public:
        STDMETHODIMP QueryInterface(REFIID riid, void** object) override
        {
            if (object == nullptr) return E_POINTER;

            if (riid == __uuidof(IUnknown) || riid == __uuidof(Interface))
            {
                *object = static_cast<Interface*>(this);
                AddRef();
                return S_OK;
            }

            *object = nullptr;
            return E_NOINTERFACE;
        }

        STDMETHODIMP_(ULONG) AddRef() override { return ++m_references; }
        STDMETHODIMP_(ULONG) Release() override { return --m_references; }

        // Includes the one the test holds
        ULONG References() const { return m_references.load(); }

    private:
        std::atomic<ULONG> m_references{ 1 };
    };

    LPWSTR CoTaskCopy(_In_ std::wstring const& text)
    {
        auto const bytes = (text.size() + 1) * sizeof(wchar_t);
        auto copy = static_cast<LPWSTR>(CoTaskMemAlloc(bytes));

        if (copy != nullptr)
        {
            memcpy(copy, text.c_str(), bytes);
        }

        return copy;
    }

    class StandInDeviceManager : public CountedStandIn<IMidiDeviceManager>
    {
    public:
        STDMETHODIMP ActivateVirtualParentDevice(ULONG, const DEVPROPERTY*, const SW_DEVICE_CREATE_INFO* createInfo, LPWSTR* createdDeviceId) override
        {
            if (createInfo == nullptr || createInfo->pszInstanceId == nullptr || createdDeviceId == nullptr) return E_INVALIDARG;

            *createdDeviceId = CoTaskCopy(std::wstring{ L"SWD\\MIDISRV\\" } + createInfo->pszInstanceId);

            return *createdDeviceId == nullptr ? E_OUTOFMEMORY : S_OK;
        }

        STDMETHODIMP DeactivateVirtualParentDevice(LPCWSTR) override { return S_OK; }

        STDMETHODIMP ActivateEndpoint(
            LPCWSTR,
            BOOL,
            MidiFlow,
            const PMIDIENDPOINTCOMMONPROPERTIES,
            ULONG,
            ULONG,
            const DEVPROPERTY*,
            const DEVPROPERTY*,
            const SW_DEVICE_CREATE_INFO*,
            LPWSTR* createdEndpointDeviceInterfaceId) override
        {
            // Nothing connects in these tests, so nothing should create an endpoint
            if (createdEndpointDeviceInterfaceId != nullptr) *createdEndpointDeviceInterfaceId = nullptr;

            return E_NOTIMPL;
        }

        STDMETHODIMP UpdateEndpointProperties(LPCWSTR, ULONG, const DEVPROPERTY*) override { return S_OK; }
        STDMETHODIMP DeleteEndpointProperties(LPCWSTR, ULONG, const DEVPROPERTY*) override { return S_OK; }
        STDMETHODIMP DeactivateEndpoint(LPCWSTR) override { return S_OK; }
        STDMETHODIMP RemoveEndpoint(LPCWSTR) override { return S_OK; }
        STDMETHODIMP RebuildMidi1PortsForEndpoint(LPCWSTR) override { return S_OK; }
        STDMETHODIMP UpdateTransportConfiguration(GUID, LPCWSTR, LPWSTR*) override { return E_NOTIMPL; }
    };

    class StandInProtocolManager : public CountedStandIn<IMidiEndpointProtocolManager>
    {
    public:
        STDMETHODIMP DiscoverAndNegotiate(GUID, LPCWSTR, ENDPOINTPROTOCOLNEGOTIATIONPARAMS) override { return E_NOTIMPL; }
        STDMETHODIMP_(BOOL) IsEnabled() override { return FALSE; }
    };
}


bool NetworkMidiShutdownTests::ClassSetup()
{
    // the transport sits beside this DLL in the build output, unless /p:TransportDll=<path> names one
    std::wstring dllPath;
    String parameter;

    if (SUCCEEDED(RuntimeParameters::TryGetValue(L"TransportDll", parameter)) && !parameter.IsEmpty())
    {
        dllPath = static_cast<wchar_t const*>(parameter);
    }
    else
    {
        std::wstring modulePath(MAX_PATH, L'\0');
        modulePath.resize(GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), modulePath.data(), static_cast<DWORD>(modulePath.size())));
        dllPath = modulePath.substr(0, modulePath.find_last_of(L'\\') + 1) + L"Midi2.NetworkMidiTransport.dll";
    }

    Log::Comment(String().Format(L"Transport: %s", dllPath.c_str()));

    // Never freed. The transport keeps state for the life of the process, as it does in the service.
    auto const module = LoadLibraryExW(dllPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    VERIFY_IS_NOT_NULL(module, L"the transport DLL loads");

    if (module == nullptr) return false;

    m_getClassObject = reinterpret_cast<GetClassObjectFunction>(GetProcAddress(module, "DllGetClassObject"));
    VERIFY_IS_NOT_NULL(m_getClassObject, L"the DLL exports DllGetClassObject");

    return m_getClassObject != nullptr;
}


_Use_decl_annotations_
void NetworkMidiShutdownTests::RunShutdown(bool const endpointManagerFirst)
{
    // Never deleted. A transport which keeps a reference releases it at DLL unload, and that must
    // fail this test rather than crash the test process.
    auto& deviceManager = *new StandInDeviceManager();
    auto& protocolManager = *new StandInProtocolManager();

    wil::com_ptr_nothrow<IClassFactory> factory;
    VERIFY_SUCCEEDED(m_getClassObject(TransportClsid, __uuidof(IClassFactory), factory.put_void()));

    wil::com_ptr_nothrow<IMidiTransport> transport;
    VERIFY_SUCCEEDED(factory->CreateInstance(nullptr, __uuidof(IMidiTransport), transport.put_void()));

    wil::com_ptr_nothrow<IMidiTransportConfigurationManager> configurationManager;
    VERIFY_SUCCEEDED(transport->Activate(__uuidof(IMidiTransportConfigurationManager), configurationManager.put_void()));
    VERIFY_SUCCEEDED(configurationManager->Initialize(TransportClsid, &deviceManager, nullptr));

    wil::com_ptr_nothrow<IMidiEndpointManager> endpointManager;
    VERIFY_SUCCEEDED(transport->Activate(__uuidof(IMidiEndpointManager), endpointManager.put_void()));
    VERIFY_SUCCEEDED(endpointManager->Initialize(&deviceManager, &protocolManager));

    VERIFY_IS_GREATER_THAN(deviceManager.References(), 1ul, L"the transport took a reference to the device manager");
    VERIFY_IS_GREATER_THAN(protocolManager.References(), 1ul, L"the transport took a reference to the protocol manager");

    if (endpointManagerFirst)
    {
        VERIFY_SUCCEEDED(endpointManager->Shutdown());
        VERIFY_SUCCEEDED(configurationManager->Shutdown());
    }
    else
    {
        VERIFY_SUCCEEDED(configurationManager->Shutdown());
        VERIFY_SUCCEEDED(endpointManager->Shutdown());
    }

    // What the service does next: it lets go of the managers, then destroys its own objects
    endpointManager.reset();
    configurationManager.reset();
    transport.reset();
    factory.reset();

    Log::Comment(String().Format(L"References left: device manager %u, protocol manager %u",
        deviceManager.References(), protocolManager.References()));

    VERIFY_ARE_EQUAL(deviceManager.References(), 1ul, L"the transport released every reference to the device manager");
    VERIFY_ARE_EQUAL(protocolManager.References(), 1ul, L"the transport released every reference to the protocol manager");
}


void NetworkMidiShutdownTests::TestShutdownEndpointManagerFirstReleasesServiceObjects()
{
    RunShutdown(true);
}

void NetworkMidiShutdownTests::TestShutdownConfigurationManagerFirstReleasesServiceObjects()
{
    RunShutdown(false);
}
