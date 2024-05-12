// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <queue>
#include <mutex>

struct ProtocolManagerWork
{
    std::wstring EndpointInstanceId{};
    bool PreferToSendJRTimestampsToEndpoint{ false };
    bool PreferToReceiveJRTimestampsFromEndpoint{ false };
    uint8_t PreferredMidiProtocol{};
    uint16_t TimeoutMS{ 5000 };

    wil::com_ptr_nothrow<IMidiBiDi> Endpoint;

    bool AlreadyTriedToNegotiationOnce{ false };
    
    bool TaskEndpointInfoReceived{ false };
    bool TaskFinalStreamNegotiationResponseReceived{ false };
    bool TaskEndpointNameReceived{ false };
    bool TaskEndpointProductInstanceIdReceived{ false };
    bool TaskDeviceIdentityReceived{ false };

    uint8_t DeclaredFunctionBlockCount{ 0 };

    uint8_t CountFunctionBlocksReceived{ 0 };
    uint8_t CountFunctionBlockNamesReceived{ 0 };
};





class CMidiEndpointProtocolManager : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiEndpointProtocolManagerInterface, 
    IMidiCallback>
{
public:
    CMidiEndpointProtocolManager() = default;
    ~CMidiEndpointProtocolManager() {}

    STDMETHOD(Initialize)(
        _In_ std::shared_ptr<CMidiClientManager>& ClientManager,
        _In_ std::shared_ptr<CMidiDeviceManager>& DeviceManager,
        _In_ std::shared_ptr<CMidiSessionTracker>& SessionTracker
    );

    STDMETHOD(NegotiateAndRequestMetadata)(
            _In_ LPCWSTR DeviceInterfaceId,
            _In_ BOOL PreferToSendJRTimestampsToEndpoint,
            _In_ BOOL PreferToReceiveJRTimestampsFromEndpoint,
            _In_ BYTE PreferredMidiProtocol,
            _In_ WORD TimeoutMS,
            _Out_ PENDPOINTPROTOCOLNEGOTIATIONRESULTS* NegotiationResults
        );

    STDMETHOD(Callback)(_In_ PVOID Data, _In_ UINT Size, _In_ LONGLONG Position, _In_ LONGLONG Context);

    HRESULT Cleanup();

private:
    void ThreadWorker();

    GUID m_sessionId;

    HRESULT ProcessCurrentWorkEntry();


    HRESULT RequestAllFunctionBlocks();
    HRESULT RequestAllEndpointDiscoveryInformation();
    HRESULT ProcessStreamConfigurationRequest(_In_ internal::PackedUmp128 ump);

    std::shared_ptr<CMidiClientManager> m_clientManager;
    std::shared_ptr<CMidiDeviceManager> m_deviceManager;
    std::shared_ptr<CMidiSessionTracker> m_sessionTracker;


    wil::com_ptr_nothrow<IMidiAbstraction> m_serviceAbstraction;

    std::mutex m_queueMutex;
    std::queue<ProtocolManagerWork> m_workQueue;

    std::thread m_queueWorkerThread;

    ProtocolManagerWork m_currentWorkItem;

    // true if we're closing down
    bool m_shutdown{ false };
};
