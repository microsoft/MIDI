// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
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
    uint16_t TimeoutMS{ 2000 };

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


class CMidiEndpointMetadataListener : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiCallback>
{
public:
    CMidiEndpointMetadataListener() = default;
    ~CMidiEndpointMetadataListener() {}

    STDMETHOD(Initialize)(
        _In_ LPCWSTR DeviceInterfaceId,
        _In_ std::shared_ptr<CMidiClientManager>& ClientManager,
        _In_ std::shared_ptr<CMidiDeviceManager>& DeviceManager,
        _In_ std::shared_ptr<CMidiSessionTracker>& SessionTracker
        );

    STDMETHOD(ListenForMetadata)();

    STDMETHOD(Callback)(_In_ PVOID Data, _In_ UINT Size, _In_ LONGLONG Position, _In_ LONGLONG Context);

    STDMETHOD(Cleanup)();

private:
    std::wstring m_deviceInterfaceId;
    std::wstring m_deviceInstanceId;
    GUID m_sessionId;

    LONGLONG m_context{ 0 };


    //wil::com_ptr_nothrow<IMidiDeviceManagerInterface> m_MidiDeviceManager;
    std::shared_ptr<CMidiClientManager> m_clientManager;
    std::shared_ptr<CMidiDeviceManager> m_deviceManager;
    std::shared_ptr<CMidiSessionTracker> m_sessionTracker;


    // these are holding locations while names are built
    std::wstring m_endpointName{};
    std::wstring m_productInstanceId{};
    std::map<uint8_t /* function block number */, std::wstring> m_functionBlockNames;


    DEVPROPKEY FunctionBlockPropertyKeyFromNumber(_In_ uint8_t functionBlockNumber);
    DEVPROPKEY FunctionBlockNamePropertyKeyFromNumber(_In_ uint8_t functionBlockNumber);

    HRESULT UpdateEndpointNameProperty();
    HRESULT UpdateEndpointProductInstanceIdProperty();
    HRESULT UpdateFunctionBlockNameProperty(_In_ uint8_t functionBlockNumber, _In_ std::wstring value);

    HRESULT UpdateDeviceIdentityProperty(_In_ internal::PackedUmp128& identityMessage);
    HRESULT UpdateEndpointInfoProperties(_In_ internal::PackedUmp128& endpointInfoNotificationMessage);
    HRESULT UpdateStreamConfigurationProperties(_In_ internal::PackedUmp128& endpointStreamConfigurationNotificationMessage);
    HRESULT UpdateFunctionBlockProperty(_In_ internal::PackedUmp128& functionBlockInfoNotificationMessage);

    HRESULT HandleFunctionBlockNameMessage(_In_ internal::PackedUmp128& functionBlockNameMessage);
    HRESULT HandleEndpointNameMessage(_In_ internal::PackedUmp128& endpointNameMessage);
    HRESULT HandleProductInstanceIdMessage(_In_ internal::PackedUmp128& productInstanceIdMessage);

    HRESULT ProcessStreamMessage(_In_ internal::PackedUmp128 ump);
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
            _In_ BOOL ContinueCapturingMetadataAsyncAfterNegotiation
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
    wil::unique_event_nothrow m_allMessagesReceived;
    wil::unique_event_nothrow m_queueWorkerThreadWakeup;

    // true if we're closing down
    bool m_shutdown{ false };
};
