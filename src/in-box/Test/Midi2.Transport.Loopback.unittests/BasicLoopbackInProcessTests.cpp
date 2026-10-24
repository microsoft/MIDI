// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <mutex>
#include <thread>

#include "BasicLoopbackInProcessTests.h"
#include "..\..\Transport\BasicLoopbackMidiTransport\basic_loopback_transport_error_codes.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    // Midi2BasicLoopbackMidiTransport in Midi2BasicLoopbackMidiTransport.idl
    constexpr GUID TransportClsid{ 0x10088473, 0x9478, 0x4e62, { 0x85, 0x0b, 0x3d, 0x23, 0x15, 0xe1, 0x35, 0xb8 } };

    using GetClassObjectFunction = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);

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

    // Behaves like the service where it matters here: an instance id already in its port list is
    // refused with S_FALSE and no interface id, and removal goes by the start of the instance id.
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

            // the real one takes about a second, which is what leaves room for a race
            if (m_activateDelayMilliseconds > 0)
            {
                Sleep(m_activateDelayMilliseconds);
            }

            std::wstring const instanceId{ createInfo->pszInstanceId };

            auto lock = std::scoped_lock(m_lock);

            if (std::find(m_ports.begin(), m_ports.end(), instanceId) != m_ports.end())
            {
                m_alreadyActivatedCalls++;
                return S_FALSE;
            }

            m_ports.push_back(instanceId);

            *createdEndpointDeviceInterfaceId = CoTaskCopy(
                std::wstring{ L"\\\\?\\SWD#MIDISRV#" } + instanceId + L"#{e7cce071-3c03-423f-88d3-f1045d02552b}");

            return *createdEndpointDeviceInterfaceId == nullptr ? E_OUTOFMEMORY : S_OK;
        }

        STDMETHODIMP UpdateEndpointProperties(LPCWSTR, ULONG, const DEVPROPERTY*) override { return S_OK; }
        STDMETHODIMP DeleteEndpointProperties(LPCWSTR, ULONG, const DEVPROPERTY*) override { return S_OK; }
        STDMETHODIMP DeactivateEndpoint(LPCWSTR) override { return S_OK; }

        STDMETHODIMP RemoveEndpoint(LPCWSTR instanceId) override
        {
            if (instanceId == nullptr) return E_INVALIDARG;

            std::wstring const prefix{ instanceId };

            auto lock = std::scoped_lock(m_lock);

            std::erase_if(m_ports, [&](std::wstring const& port) { return port.starts_with(prefix); });

            return S_OK;
        }

        STDMETHODIMP RebuildMidi1PortsForEndpoint(LPCWSTR) override { return S_OK; }
        STDMETHODIMP UpdateTransportConfiguration(GUID, LPCWSTR, LPWSTR*) override { return E_NOTIMPL; }

        // An endpoint the service has, that the transport has no record of
        void AddOrphan(_In_ std::wstring const& instanceId)
        {
            auto lock = std::scoped_lock(m_lock);
            m_ports.push_back(instanceId);
        }

        size_t PortCount()
        {
            auto lock = std::scoped_lock(m_lock);
            return m_ports.size();
        }

        std::atomic<uint32_t> m_activateCalls{ 0 };
        std::atomic<uint32_t> m_alreadyActivatedCalls{ 0 };
        DWORD m_activateDelayMilliseconds{ 0 };

    private:
        std::mutex m_lock;
        std::vector<std::wstring> m_ports;
    };

    class StandInProtocolManager : public CountedStandIn<IMidiEndpointProtocolManager>
    {
    public:
        STDMETHODIMP DiscoverAndNegotiate(GUID, LPCWSTR, ENDPOINTPROTOCOLNEGOTIATIONPARAMS) override { return E_NOTIMPL; }
        STDMETHODIMP_(BOOL) IsEnabled() override { return FALSE; }
    };


    struct ConfigResult
    {
        HRESULT Result{ E_FAIL };
        bool HasResponse{ false };
        bool Success{ false };
        uint32_t ErrorCode{ 0 };
        std::wstring Json{};
    };

    ConfigResult SendConfig(_In_ IMidiTransportConfigurationManager* manager, _In_ std::wstring const& json)
    {
        ConfigResult result{};

        LPWSTR response{ nullptr };
        result.Result = manager->UpdateConfiguration(json.c_str(), &response);

        if (response != nullptr)
        {
            result.Json = response;
            CoTaskMemFree(response);

            winrt::Windows::Data::Json::JsonObject parsed{ nullptr };

            if (winrt::Windows::Data::Json::JsonObject::TryParse(result.Json, parsed))
            {
                result.HasResponse = true;
                result.Success = parsed.GetNamedBoolean(L"success", false);
                result.ErrorCode = static_cast<uint32_t>(parsed.GetNamedNumber(L"errorCode", 0));
            }
        }

        return result;
    }

    std::wstring CreateJson(
        _In_ std::wstring const& associationId,
        _In_ std::wstring const& uniqueId,
        _In_ std::wstring const& name)
    {
        return L"{\"create\":{\"" + associationId + L"\":{\"endpoint\":{\"name\":\"" + name +
            L"\",\"uniqueIdentifier\":\"" + uniqueId + L"\"}}}}";
    }

    std::wstring NewGuidString()
    {
        GUID guid{};
        (void)CoCreateGuid(&guid);

        wchar_t text[64]{};
        (void)StringFromGUID2(guid, text, ARRAYSIZE(text));

        return text;
    }

    size_t ListedLoopbackCount(_In_ IMidiTransportConfigurationManager* manager)
    {
        auto const result = SendConfig(manager, LR"({"transportCommand":{"commandName":"listEntries"}})");

        winrt::Windows::Data::Json::JsonObject parsed{ nullptr };

        if (!winrt::Windows::Data::Json::JsonObject::TryParse(result.Json, parsed) || !parsed.HasKey(L"entries"))
        {
            return SIZE_MAX;
        }

        return parsed.GetNamedArray(L"entries").Size();
    }

    // A transport in the state the service leaves it in after startup. Shut down when it goes,
    // so the next test starts from an empty transport state.
    struct LoadedTransport
    {
        StandInDeviceManager& DeviceManager{ *new StandInDeviceManager() };
        StandInProtocolManager& ProtocolManager{ *new StandInProtocolManager() };

        wil::com_ptr_nothrow<IMidiTransport> Transport;
        wil::com_ptr_nothrow<IMidiTransportConfigurationManager> ConfigurationManager;
        wil::com_ptr_nothrow<IMidiEndpointManager> EndpointManager;

        void Load(_In_ GetClassObjectFunction getClassObject)
        {
            wil::com_ptr_nothrow<IClassFactory> factory;
            VERIFY_SUCCEEDED(getClassObject(TransportClsid, __uuidof(IClassFactory), factory.put_void()));
            VERIFY_SUCCEEDED(factory->CreateInstance(nullptr, __uuidof(IMidiTransport), Transport.put_void()));

            VERIFY_SUCCEEDED(Transport->Activate(__uuidof(IMidiTransportConfigurationManager), ConfigurationManager.put_void()));
            VERIFY_SUCCEEDED(ConfigurationManager->Initialize(TransportClsid, &DeviceManager, nullptr));
        }

        void StartEndpointManager()
        {
            VERIFY_SUCCEEDED(Transport->Activate(__uuidof(IMidiEndpointManager), EndpointManager.put_void()));
            VERIFY_SUCCEEDED(EndpointManager->Initialize(&DeviceManager, &ProtocolManager));
        }

        ~LoadedTransport()
        {
            if (EndpointManager) LOG_IF_FAILED(EndpointManager->Shutdown());
            if (ConfigurationManager) LOG_IF_FAILED(ConfigurationManager->Shutdown());
        }
    };
}


bool BasicLoopbackInProcessTests::ClassSetup()
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
        dllPath = modulePath.substr(0, modulePath.find_last_of(L'\\') + 1) + L"Midi2.BasicLoopbackMidiTransport.dll";
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


void BasicLoopbackInProcessTests::TestReusedAssociationIdIsRefused()
{
    LoadedTransport loaded;
    loaded.Load(m_getClassObject);
    loaded.StartEndpointManager();

    auto const association = NewGuidString();
    auto const otherAssociation = NewGuidString();

    auto const first = SendConfig(loaded.ConfigurationManager.get(), CreateJson(association, L"ReuseX", L"Reuse test X"));
    VERIFY_IS_TRUE(first.Success, first.Json.c_str());

    // Used to replace the first loopback in the transport's table and leave it in the service
    auto const second = SendConfig(loaded.ConfigurationManager.get(), CreateJson(association, L"ReuseY", L"Reuse test Y"));
    Log::Comment(second.Json.c_str());
    VERIFY_IS_TRUE(second.HasResponse);
    VERIFY_IS_FALSE(second.Success);
    VERIFY_ARE_EQUAL(second.ErrorCode, static_cast<uint32_t>(BASIC_LOOPBACK_ERROR_CODE_INVALID_ASSOCIATION_ID));

    // ...and then this one reached the service, got S_FALSE and crashed it
    auto const third = SendConfig(loaded.ConfigurationManager.get(), CreateJson(otherAssociation, L"ReuseX", L"Reuse test X again"));
    Log::Comment(third.Json.c_str());
    VERIFY_IS_TRUE(third.HasResponse);
    VERIFY_IS_FALSE(third.Success);
    VERIFY_ARE_EQUAL(third.ErrorCode, static_cast<uint32_t>(BASIC_LOOPBACK_ERROR_CODE_DUPLICATE_UNIQUE_ID));

    VERIFY_ARE_EQUAL(loaded.DeviceManager.m_activateCalls.load(), 1u, L"only the first create reached the service");
    VERIFY_ARE_EQUAL(loaded.DeviceManager.PortCount(), static_cast<size_t>(1));
    VERIFY_ARE_EQUAL(ListedLoopbackCount(loaded.ConfigurationManager.get()), static_cast<size_t>(1));

    // the first one can still be removed
    auto const removed = SendConfig(loaded.ConfigurationManager.get(), L"{\"remove\":[\"" + association + L"\"]}");
    VERIFY_IS_TRUE(removed.Success, removed.Json.c_str());
    VERIFY_ARE_EQUAL(loaded.DeviceManager.PortCount(), static_cast<size_t>(0));
}


void BasicLoopbackInProcessTests::TestEndpointTheServiceAlreadyHasIsAnError()
{
    LoadedTransport loaded;
    loaded.Load(m_getClassObject);
    loaded.StartEndpointManager();

    // Left behind in the service by something the transport no longer tracks
    loaded.DeviceManager.AddOrphan(L"MIDIU_BLOOP_ORPHANED");

    auto const association = NewGuidString();

    auto const refused = SendConfig(loaded.ConfigurationManager.get(), CreateJson(association, L"Orphaned", L"Orphan test"));
    Log::Comment(refused.Json.c_str());
    VERIFY_IS_TRUE(refused.HasResponse);
    VERIFY_IS_FALSE(refused.Success);
    VERIFY_ARE_EQUAL(refused.ErrorCode, static_cast<uint32_t>(BASIC_LOOPBACK_ERROR_CODE_ENDPOINT_CREATION_FAILED));
    VERIFY_ARE_EQUAL(loaded.DeviceManager.m_alreadyActivatedCalls.load(), 1u, L"the service said it already had it");

    VERIFY_ARE_EQUAL(ListedLoopbackCount(loaded.ConfigurationManager.get()), static_cast<size_t>(0));
    VERIFY_ARE_EQUAL(loaded.DeviceManager.PortCount(), static_cast<size_t>(1), L"the orphan was left alone");

    // The failure released the association id and the name
    auto const retried = SendConfig(loaded.ConfigurationManager.get(), CreateJson(association, L"NotOrphaned", L"Orphan test"));
    VERIFY_IS_TRUE(retried.Success, retried.Json.c_str());
}


void BasicLoopbackInProcessTests::TestConfigFileUniqueIdsDifferingOnlyInCaseCreateOne()
{
    LoadedTransport loaded;
    loaded.Load(m_getClassObject);

    // The service hands the configuration manager the saved file before it starts the endpoint
    // manager, so both of these are queued
    auto const json =
        L"{\"create\":{"
        L"\"" + NewGuidString() + L"\":{\"endpoint\":{\"name\":\"Case one\",\"uniqueIdentifier\":\"Loop1\"}},"
        L"\"" + NewGuidString() + L"\":{\"endpoint\":{\"name\":\"Case two\",\"uniqueIdentifier\":\"loop1\"}}"
        L"}}";

    auto const queued = SendConfig(loaded.ConfigurationManager.get(), json);
    Log::Comment(queued.Json.c_str());
    VERIFY_SUCCEEDED(queued.Result);

    // Used to crash inside Initialize, on every service start
    loaded.StartEndpointManager();

    VERIFY_ARE_EQUAL(loaded.DeviceManager.m_activateCalls.load(), 1u);
    VERIFY_ARE_EQUAL(loaded.DeviceManager.m_alreadyActivatedCalls.load(), 0u);
    VERIFY_ARE_EQUAL(ListedLoopbackCount(loaded.ConfigurationManager.get()), static_cast<size_t>(1));
}


void BasicLoopbackInProcessTests::TestConcurrentCreatesOfOneUniqueIdCreateOne()
{
    LoadedTransport loaded;
    loaded.Load(m_getClassObject);
    loaded.StartEndpointManager();

    loaded.DeviceManager.m_activateDelayMilliseconds = 300;

    constexpr size_t CreatorCount{ 8 };

    std::vector<ConfigResult> results(CreatorCount);
    std::vector<std::thread> creators;

    for (size_t i = 0; i < CreatorCount; i++)
    {
        creators.emplace_back([&, i]()
            {
                auto const com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

                results[i] = SendConfig(
                    loaded.ConfigurationManager.get(),
                    CreateJson(NewGuidString(), L"SameMoment", L"Same moment " + std::to_wstring(i)));

                if (SUCCEEDED(com)) CoUninitialize();
            });
    }

    for (auto& creator : creators)
    {
        creator.join();
    }

    size_t created{ 0 };
    size_t refusedAsDuplicate{ 0 };

    for (auto const& result : results)
    {
        Log::Comment(result.Json.c_str());

        if (result.Success)
        {
            created++;
        }
        else if (result.ErrorCode == BASIC_LOOPBACK_ERROR_CODE_DUPLICATE_UNIQUE_ID)
        {
            refusedAsDuplicate++;
        }
    }

    VERIFY_ARE_EQUAL(created, static_cast<size_t>(1));
    VERIFY_ARE_EQUAL(refusedAsDuplicate, CreatorCount - 1);
    VERIFY_ARE_EQUAL(loaded.DeviceManager.m_activateCalls.load(), 1u, L"only one create reached the service");
    VERIFY_ARE_EQUAL(loaded.DeviceManager.m_alreadyActivatedCalls.load(), 0u);
}


_Use_decl_annotations_
void BasicLoopbackInProcessTests::RunShutdown(bool const endpointManagerFirst)
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

    // one loopback, so the table has something to tear down
    auto const created = SendConfig(configurationManager.get(), CreateJson(NewGuidString(), L"ShutdownTest", L"Shutdown test"));
    VERIFY_IS_TRUE(created.Success, created.Json.c_str());

    VERIFY_IS_GREATER_THAN(deviceManager.References(), 1ul, L"the transport took a reference to the device manager");

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

    endpointManager.reset();
    configurationManager.reset();
    transport.reset();
    factory.reset();

    Log::Comment(String().Format(L"References left: device manager %u, protocol manager %u",
        deviceManager.References(), protocolManager.References()));

    VERIFY_ARE_EQUAL(deviceManager.References(), 1ul, L"the transport released every reference to the device manager");
    VERIFY_ARE_EQUAL(protocolManager.References(), 1ul, L"the transport released every reference to the protocol manager");
}


void BasicLoopbackInProcessTests::TestShutdownEndpointManagerFirstReleasesServiceObjects()
{
    RunShutdown(true);
}

void BasicLoopbackInProcessTests::TestShutdownConfigurationManagerFirstReleasesServiceObjects()
{
    RunShutdown(false);
}
