// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <atomic>

#include "MidiSynthShutdownTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    // Midi2MidiSynthTransport in Midi2MidiSynthTransport.idl
    constexpr GUID TransportClsid{ 0x7605713e, 0xfea9, 0x409d, { 0xa9, 0x0f, 0xa8, 0x12, 0x33, 0x20, 0x0d, 0x0a } };

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
            const SW_DEVICE_CREATE_INFO* createInfo,
            LPWSTR* createdEndpointDeviceInterfaceId) override
        {
            if (createdEndpointDeviceInterfaceId == nullptr || createInfo == nullptr || createInfo->pszInstanceId == nullptr) return E_INVALIDARG;

            *createdEndpointDeviceInterfaceId = nullptr;

            m_activateCalls++;

            // what the service answers for an instance id it already has
            if (ServiceAlreadyHasEndpoint)
            {
                return S_FALSE;
            }

            *createdEndpointDeviceInterfaceId = CoTaskCopy(
                std::wstring{ L"\\\\?\\SWD#MIDISRV#" } + createInfo->pszInstanceId + L"#{e7cce071-3c03-423f-88d3-f1045d02552b}");

            return *createdEndpointDeviceInterfaceId == nullptr ? E_OUTOFMEMORY : S_OK;
        }

        STDMETHODIMP UpdateEndpointProperties(LPCWSTR, ULONG, const DEVPROPERTY*) override { return S_OK; }
        STDMETHODIMP DeleteEndpointProperties(LPCWSTR, ULONG, const DEVPROPERTY*) override { return S_OK; }
        STDMETHODIMP DeactivateEndpoint(LPCWSTR) override { return S_OK; }
        STDMETHODIMP RemoveEndpoint(LPCWSTR) override { m_removeCalls++; return S_OK; }
        STDMETHODIMP RebuildMidi1PortsForEndpoint(LPCWSTR) override { return S_OK; }
        STDMETHODIMP UpdateTransportConfiguration(GUID, LPCWSTR, LPWSTR*) override { return E_NOTIMPL; }

        bool ServiceAlreadyHasEndpoint{ false };
        std::atomic<uint32_t> m_activateCalls{ 0 };
        std::atomic<uint32_t> m_removeCalls{ 0 };
    };

    class StandInProtocolManager : public CountedStandIn<IMidiEndpointProtocolManager>
    {
    public:
        STDMETHODIMP DiscoverAndNegotiate(GUID, LPCWSTR, ENDPOINTPROTOCOLNEGOTIATIONPARAMS) override
        {
            // Long enough that shutdown finds the negotiation thread still running
            m_negotiations++;
            Sleep(200);
            return E_NOTIMPL;
        }

        STDMETHODIMP_(BOOL) IsEnabled() override { return FALSE; }

        std::atomic<uint32_t> m_negotiations{ 0 };
    };

    void SetEnabled(_In_ IMidiTransportConfigurationManager* manager, _In_ bool const enabled)
    {
        LPWSTR response{ nullptr };

        VERIFY_SUCCEEDED(manager->UpdateConfiguration(enabled ? LR"({"enabled":true})" : LR"({"enabled":false})", &response));

        if (response != nullptr)
        {
            Log::Comment(response);
            CoTaskMemFree(response);
        }
    }
}


bool MidiSynthShutdownTests::ClassSetup()
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
        dllPath = modulePath.substr(0, modulePath.find_last_of(L'\\') + 1) + L"Midi2.MidiSynthTransport.dll";
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
void MidiSynthShutdownTests::RunShutdown(ShutdownOrder const order)
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

    // switched on, so Initialize creates the endpoint and starts negotiating with it
    SetEnabled(configurationManager.get(), true);

    wil::com_ptr_nothrow<IMidiEndpointManager> endpointManager;
    VERIFY_SUCCEEDED(transport->Activate(__uuidof(IMidiEndpointManager), endpointManager.put_void()));
    VERIFY_SUCCEEDED(endpointManager->Initialize(&deviceManager, &protocolManager));

    VERIFY_ARE_EQUAL(deviceManager.m_activateCalls.load(), 1u, L"the endpoint was created");
    VERIFY_IS_GREATER_THAN(deviceManager.References(), 1ul, L"the transport took a reference to the device manager");
    VERIFY_IS_GREATER_THAN(protocolManager.References(), 1ul, L"the transport took a reference to the protocol manager");

    if (order == ShutdownOrder::EndpointManagerFirst)
    {
        VERIFY_SUCCEEDED(endpointManager->Shutdown());
        VERIFY_SUCCEEDED(configurationManager->Shutdown());
    }
    else
    {
        VERIFY_SUCCEEDED(configurationManager->Shutdown());

        // Before the endpoint manager is shut down: the configuration manager's shutdown alone
        // waits for the negotiation and lets go of everything
        Log::Comment(String().Format(L"After the configuration manager only: device manager %u, protocol manager %u",
            deviceManager.References(), protocolManager.References()));

        VERIFY_ARE_EQUAL(deviceManager.References(), 1ul);
        VERIFY_ARE_EQUAL(protocolManager.References(), 1ul);

        VERIFY_SUCCEEDED(endpointManager->Shutdown());
    }

    // What the service does next: it lets go of the managers, then destroys its own objects
    endpointManager.reset();
    configurationManager.reset();
    transport.reset();
    factory.reset();

    Log::Comment(String().Format(L"References left: device manager %u, protocol manager %u, negotiations %u",
        deviceManager.References(), protocolManager.References(), protocolManager.m_negotiations.load()));

    VERIFY_ARE_EQUAL(deviceManager.References(), 1ul, L"the transport released every reference to the device manager");
    VERIFY_ARE_EQUAL(protocolManager.References(), 1ul, L"the transport released every reference to the protocol manager");
}


void MidiSynthShutdownTests::TestShutdownEndpointManagerFirstReleasesServiceObjects()
{
    RunShutdown(ShutdownOrder::EndpointManagerFirst);
}


void MidiSynthShutdownTests::TestShutdownConfigurationManagerFirstReleasesServiceObjects()
{
    RunShutdown(ShutdownOrder::ConfigurationManagerFirst);
}


void MidiSynthShutdownTests::TestEndpointTheServiceAlreadyHasFailsInitializeCleanly()
{
    auto& deviceManager = *new StandInDeviceManager();
    auto& protocolManager = *new StandInProtocolManager();

    deviceManager.ServiceAlreadyHasEndpoint = true;

    wil::com_ptr_nothrow<IClassFactory> factory;
    VERIFY_SUCCEEDED(m_getClassObject(TransportClsid, __uuidof(IClassFactory), factory.put_void()));

    wil::com_ptr_nothrow<IMidiTransport> transport;
    VERIFY_SUCCEEDED(factory->CreateInstance(nullptr, __uuidof(IMidiTransport), transport.put_void()));

    wil::com_ptr_nothrow<IMidiTransportConfigurationManager> configurationManager;
    VERIFY_SUCCEEDED(transport->Activate(__uuidof(IMidiTransportConfigurationManager), configurationManager.put_void()));
    VERIFY_SUCCEEDED(configurationManager->Initialize(TransportClsid, &deviceManager, nullptr));

    SetEnabled(configurationManager.get(), true);

    wil::com_ptr_nothrow<IMidiEndpointManager> endpointManager;
    VERIFY_SUCCEEDED(transport->Activate(__uuidof(IMidiEndpointManager), endpointManager.put_void()));

    auto const initializeResult = endpointManager->Initialize(&deviceManager, &protocolManager);
    Log::Comment(String().Format(L"Initialize: 0x%08X", initializeResult));

    VERIFY_ARE_EQUAL(initializeResult, HRESULT_FROM_WIN32(ERROR_DEVICE_ALREADY_ATTACHED));
    VERIFY_ARE_EQUAL(deviceManager.m_activateCalls.load(), 1u);
    VERIFY_ARE_EQUAL(protocolManager.m_negotiations.load(), 0u, L"nothing negotiated with an endpoint that wasn't created");

    // The service won't shut down an endpoint manager whose Initialize failed, so it has already
    // let go. Only the configuration manager still holds a reference.
    VERIFY_ARE_EQUAL(deviceManager.References(), 2ul, L"only the configuration manager holds the device manager");
    VERIFY_ARE_EQUAL(protocolManager.References(), 1ul, L"the failed endpoint manager let go of the protocol manager");

    // A settings change after the failure must not reach the failed endpoint manager
    SetEnabled(configurationManager.get(), false);
    SetEnabled(configurationManager.get(), true);
    VERIFY_ARE_EQUAL(deviceManager.m_activateCalls.load(), 1u);

    // The service only shuts down the configuration manager
    VERIFY_SUCCEEDED(configurationManager->Shutdown());

    endpointManager.reset();
    configurationManager.reset();
    transport.reset();
    factory.reset();

    VERIFY_ARE_EQUAL(deviceManager.References(), 1ul, L"the transport released every reference to the device manager");
    VERIFY_ARE_EQUAL(protocolManager.References(), 1ul, L"the transport released every reference to the protocol manager");
}
