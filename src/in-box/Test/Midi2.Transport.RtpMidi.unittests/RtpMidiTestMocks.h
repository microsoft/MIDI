// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Stand-ins for the service interfaces the transport calls, so it can run inside the test
// process without the MIDI service, admin rights or a deployed plugin.

namespace RtpMidiTest
{
    inline bool SameText(std::wstring const& left, std::wstring const& right)
    {
        return _wcsicmp(left.c_str(), right.c_str()) == 0;
    }

    inline LPWSTR CoTaskCopy(std::wstring const& text)
    {
        auto const bytes = (text.size() + 1) * sizeof(wchar_t);
        auto copy = static_cast<LPWSTR>(CoTaskMemAlloc(bytes));
        if (copy != nullptr) memcpy(copy, text.c_str(), bytes);
        return copy;
    }

    // Plain reference counting is enough: every mock outlives the transport
    template <typename Interface>
    class MockBase : public Interface
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

    private:
        std::atomic<ULONG> m_references{ 1 };
    };

    struct ActivatedEndpoint
    {
        std::wstring InstanceId;
        std::wstring InterfaceId;
        std::wstring EndpointName;
        std::wstring UniqueIdentifier;
        std::wstring TransportCode;
        GUID TransportId{};
        MidiDataFormats NativeFormat{ MidiDataFormats_Invalid };
        ULONG PropertyCount{ 0 };
        bool Removed{ false };
    };

    class MockDeviceManager : public MockBase<IMidiDeviceManager>
    {
    public:
        STDMETHODIMP ActivateVirtualParentDevice(ULONG, const DEVPROPERTY*, const SW_DEVICE_CREATE_INFO* createInfo, LPWSTR* createdDeviceId) override
        {
            if (createInfo == nullptr || createInfo->pszInstanceId == nullptr || createdDeviceId == nullptr) return E_INVALIDARG;

            auto lock = std::scoped_lock{ m_lock };
            m_parentId = std::wstring{ L"SWD\\MIDISRV\\" } + createInfo->pszInstanceId;
            *createdDeviceId = CoTaskCopy(m_parentId);

            return *createdDeviceId == nullptr ? E_OUTOFMEMORY : S_OK;
        }

        STDMETHODIMP DeactivateVirtualParentDevice(LPCWSTR) override { return S_OK; }

        STDMETHODIMP ActivateEndpoint(
            LPCWSTR parentInstanceId,
            BOOL,
            MidiFlow,
            const PMIDIENDPOINTCOMMONPROPERTIES common,
            ULONG intPropertyCount,
            ULONG,
            const DEVPROPERTY*,
            const DEVPROPERTY*,
            const SW_DEVICE_CREATE_INFO* createInfo,
            LPWSTR* createdEndpointDeviceInterfaceId) override
        {
            if (common == nullptr || createInfo == nullptr || createInfo->pszInstanceId == nullptr || createdEndpointDeviceInterfaceId == nullptr) return E_INVALIDARG;

            *createdEndpointDeviceInterfaceId = nullptr;

            std::function<void(std::wstring const&)> duringActivation{ nullptr };
            std::wstring interfaceId{ };

            {
                auto lock = std::scoped_lock{ m_lock };

                if (parentInstanceId == nullptr || !SameText(parentInstanceId, m_parentId)) m_wrongParent = true;

                std::wstring const instanceId{ createInfo->pszInstanceId };

                // the real device manager reports an instance id which is already active this way
                for (auto const& existing : m_endpoints)
                {
                    if (!existing.Removed && SameText(existing.InstanceId, instanceId)) return S_FALSE;
                }

                ActivatedEndpoint endpoint{};
                endpoint.InstanceId = instanceId;
                endpoint.InterfaceId = L"\\\\?\\SWD#MIDISRV#" + instanceId + L"#{e7cce071-3c03-423f-88d3-f1045d02552b}";
                endpoint.EndpointName = common->EndpointName != nullptr ? common->EndpointName : L"";
                endpoint.UniqueIdentifier = common->UniqueIdentifier != nullptr ? common->UniqueIdentifier : L"";
                endpoint.TransportCode = common->TransportCode != nullptr ? common->TransportCode : L"";
                endpoint.TransportId = common->TransportId;
                endpoint.NativeFormat = common->NativeDataFormat;
                endpoint.PropertyCount = intPropertyCount;

                *createdEndpointDeviceInterfaceId = CoTaskCopy(endpoint.InterfaceId);
                m_endpoints.push_back(endpoint);

                interfaceId = endpoint.InterfaceId;
                duringActivation = std::exchange(m_duringNextActivation, nullptr);
            }

            // The real service makes the endpoint visible to apps, and builds its MIDI 1.0 ports,
            // before this returns. An app can open it in that time.
            if (duringActivation != nullptr) duringActivation(interfaceId);

            return S_OK;
        }

        // Runs once, inside the next activation, with the new endpoint's interface id
        void DuringNextActivation(_In_ std::function<void(std::wstring const&)> action)
        {
            auto lock = std::scoped_lock{ m_lock };
            m_duringNextActivation = std::move(action);
        }

        STDMETHODIMP UpdateEndpointProperties(LPCWSTR endpointDeviceInterfaceId, ULONG count, const DEVPROPERTY* properties) override
        {
            if (endpointDeviceInterfaceId == nullptr || properties == nullptr) return E_INVALIDARG;

            auto lock = std::scoped_lock{ m_lock };

            for (ULONG i = 0; i < count; i++)
            {
                // PKEY_MIDI_MidiOutCalculatedLatencyTicks is property 800 in the MIDI set
                if (properties[i].CompKey.Key.pid == 800 && properties[i].BufferSize == sizeof(uint64_t) && properties[i].Buffer != nullptr)
                {
                    m_latencyWrites.emplace_back(endpointDeviceInterfaceId, *static_cast<uint64_t const*>(properties[i].Buffer));
                }
            }

            return S_OK;
        }

        STDMETHODIMP DeleteEndpointProperties(LPCWSTR, ULONG, const DEVPROPERTY*) override { return S_OK; }
        STDMETHODIMP DeactivateEndpoint(LPCWSTR) override { return S_OK; }

        STDMETHODIMP RemoveEndpoint(LPCWSTR instanceId) override
        {
            if (instanceId == nullptr) return E_INVALIDARG;

            auto lock = std::scoped_lock{ m_lock };

            for (auto& endpoint : m_endpoints)
            {
                if (!endpoint.Removed && SameText(endpoint.InstanceId, instanceId))
                {
                    endpoint.Removed = true;
                    return S_OK;
                }
            }

            m_unknownRemovals++;
            return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        }

        STDMETHODIMP RebuildMidi1PortsForEndpoint(LPCWSTR) override { return S_OK; }
        STDMETHODIMP UpdateTransportConfiguration(GUID, LPCWSTR, LPWSTR*) override { return E_NOTIMPL; }

        std::vector<ActivatedEndpoint> Endpoints()
        {
            auto lock = std::scoped_lock{ m_lock };
            return m_endpoints;
        }

        size_t LatencyWritesFor(std::wstring const& interfaceId, uint64_t& lastValue)
        {
            auto lock = std::scoped_lock{ m_lock };

            size_t count = 0;
            for (auto const& write : m_latencyWrites)
            {
                if (SameText(write.first, interfaceId))
                {
                    count++;
                    lastValue = write.second;
                }
            }

            return count;
        }

        bool HadParent() { auto lock = std::scoped_lock{ m_lock }; return !m_parentId.empty(); }
        bool WrongParent() { auto lock = std::scoped_lock{ m_lock }; return m_wrongParent; }
        uint32_t UnknownRemovals() { auto lock = std::scoped_lock{ m_lock }; return m_unknownRemovals; }

    private:
        std::mutex m_lock;
        std::wstring m_parentId;
        std::vector<ActivatedEndpoint> m_endpoints;
        std::vector<std::pair<std::wstring, uint64_t>> m_latencyWrites;
        std::function<void(std::wstring const&)> m_duringNextActivation{ nullptr };
        bool m_wrongParent{ false };
        uint32_t m_unknownRemovals{ 0 };
    };

    class MockProtocolManager : public MockBase<IMidiEndpointProtocolManager>
    {
    public:
        STDMETHODIMP DiscoverAndNegotiate(GUID, LPCWSTR, ENDPOINTPROTOCOLNEGOTIATIONPARAMS) override
        {
            m_calls++;
            return E_NOTIMPL;
        }

        STDMETHODIMP_(BOOL) IsEnabled() override { return FALSE; }

        std::atomic<uint32_t> m_calls{ 0 };
    };

    // What the service's client pipe would receive from the endpoint
    class MockCallback : public MockBase<IMidiCallback>
    {
    public:
        STDMETHODIMP Callback(MessageOptionFlags, PVOID message, UINT size, LONGLONG position, LONGLONG context) override
        {
            LARGE_INTEGER now{};
            QueryPerformanceCounter(&now);

            if (message == nullptr || size < sizeof(uint32_t)) return E_INVALIDARG;

            auto lock = std::scoped_lock{ m_lock };

            auto const words = static_cast<uint32_t const*>(message);
            m_words.insert(m_words.end(), words, words + size / sizeof(uint32_t));

            if (context != m_expectedContext) m_wrongContext = true;
            if (position <= 0 || position < m_lastPosition) m_badPosition = true;
            if (position > now.QuadPart) m_stampedAfterArrival = true;
            m_lastPosition = position;

            return S_OK;
        }

        void Expect(LONGLONG const context) { auto lock = std::scoped_lock{ m_lock }; m_expectedContext = context; }
        std::vector<uint32_t> Words() { auto lock = std::scoped_lock{ m_lock }; return m_words; }
        bool WrongContext() { auto lock = std::scoped_lock{ m_lock }; return m_wrongContext; }
        bool BadPosition() { auto lock = std::scoped_lock{ m_lock }; return m_badPosition; }
        bool StampedAfterArrival() { auto lock = std::scoped_lock{ m_lock }; return m_stampedAfterArrival; }

    private:
        std::mutex m_lock;
        std::vector<uint32_t> m_words;
        LONGLONG m_expectedContext{ 0 };
        LONGLONG m_lastPosition{ 0 };
        bool m_wrongContext{ false };
        bool m_badPosition{ false };
        bool m_stampedAfterArrival{ false };
    };

    // An endpoint opened the way the service opens one. Shut down however the test ends.
    class OpenedEndpoint
    {
    public:
        OpenedEndpoint(IMidiTransport* transport, std::wstring const& interfaceId, LONGLONG const context)
        {
            m_callback.Expect(context);

            if (FAILED(transport->Activate(__uuidof(IMidiBidirectional), reinterpret_cast<void**>(&m_bidi))) || m_bidi == nullptr) return;

            DWORD taskId{ 0 };
            m_opened = SUCCEEDED(m_bidi->Initialize(interfaceId.c_str(), nullptr, &taskId, &m_callback, context, GUID{}));
        }

        ~OpenedEndpoint()
        {
            if (m_bidi == nullptr) return;

            m_bidi->Shutdown();
            m_bidi->Release();
        }

        OpenedEndpoint(OpenedEndpoint const&) = delete;
        OpenedEndpoint& operator=(OpenedEndpoint const&) = delete;

        bool IsOpen() const { return m_opened; }
        MockCallback& Received() { return m_callback; }

        HRESULT Send(uint32_t const* words, UINT const bytes)
        {
            return m_bidi->SendMidiMessage(MessageOptionFlags_None, const_cast<uint32_t*>(words), bytes, 0);
        }

    private:
        MockCallback m_callback;
        IMidiBidirectional* m_bidi{ nullptr };
        bool m_opened{ false };
    };
}
