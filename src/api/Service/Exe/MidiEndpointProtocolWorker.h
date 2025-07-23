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

struct MidiFunctionBlockName
{
    std::wstring Name;
    bool IsComplete{ false };
};


class CMidiEndpointProtocolWorker : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiCallback>
{
public:
    CMidiEndpointProtocolWorker() = default;
    ~CMidiEndpointProtocolWorker() {}

    STDMETHOD(Initialize)(
        _In_ GUID sessionId,
        _In_ GUID transportId,
        _In_ LPCWSTR endpointDeviceInterfaceId,
        _In_ std::shared_ptr<CMidiClientManager>& clientManager,
        _In_ std::shared_ptr<CMidiDeviceManager>& deviceManager,
        _In_ std::shared_ptr<CMidiSessionTracker>& sessionTracker
        );

    STDMETHOD(Start)(
        _In_ ENDPOINTPROTOCOLNEGOTIATIONPARAMS negotiationParams
        );

    STDMETHOD(Callback)(_In_ MessageOptionFlags, _In_ PVOID data, _In_ UINT size, _In_ LONGLONG position, _In_ LONGLONG context);

    STDMETHOD(Shutdown)();

    void EndProcessing() { if (m_endProcessing.is_valid() && !m_endProcessing.is_signaled()) m_endProcessing.SetEvent(); }

private:
    std::wstring m_endpointDeviceInterfaceId;
    //std::wstring m_deviceInstanceId;
    GUID m_sessionId{};
    DWORD m_clientProcessId{};
    GUID m_transportId{};

    LONGLONG m_context{ 0 };

    bool m_inInitialFunctionBlockDiscovery{ false };
    DWORD m_discoveryTimeoutMS{ MIDI_DISCOVERY_TIMEOUT_DEFAULT_VALUE };

    wil::unique_event_nothrow m_endProcessing;
    wil::unique_event_nothrow m_allInitialDiscoveryAndNegotiationMessagesReceived;
    //wil::unique_event_nothrow m_queueWorkerThreadWakeup;

    // true if we're closing down
 //   bool m_shutdown{ false };

    //wil::com_ptr_nothrow<IMidiDeviceManager> m_MidiDeviceManager;
    std::shared_ptr<CMidiClientManager> m_clientManager;
    std::shared_ptr<CMidiDeviceManager> m_deviceManager;
    std::shared_ptr<CMidiSessionTracker> m_sessionTracker;

    //wil::com_ptr_nothrow<IMidiProtocolNegotiationCompleteCallback> m_negotiationCompleteCallback{ nullptr };

    //MIDISRV_CLIENT m_midiClient{ };

    //std::shared_ptr<CMidiClientPipe> m_clientPipe{ nullptr };
    wil::com_ptr_nothrow<IMidiBidirectional> m_midiBidiDevice;


    // these are holding locations while names are built
    std::wstring m_endpointName{};
    std::wstring m_productInstanceId{};
    std::map<uint8_t /* function block number */, MidiFunctionBlockName> m_functionBlockNames{ };
    std::map<uint8_t /* function block number */, MidiFunctionBlockProperty> m_functionBlocks{ };



    bool m_preferToSendJRTimestampsToEndpoint{ false };
    bool m_preferToReceiveJRTimestampsFromEndpoint{ false };
    uint8_t m_preferredMidiProtocol{};

    bool m_alreadyTriedToNegotiationOnce{ false };

    bool m_timedOut{ false };

    bool m_taskEndpointInfoReceived{ false };
    bool m_taskFinalStreamNegotiationResponseReceived{ false };
    bool m_taskEndpointNameReceived{ false };
    bool m_taskEndpointProductInstanceIdReceived{ false };
    bool m_taskDeviceIdentityReceived{ false };

    bool m_functionBlocksAreStatic{ false };
    uint8_t m_declaredFunctionBlockCount{ 0 };

    HRESULT CheckIfDiscoveryComplete();
    HRESULT SetDiscoveryCompleteProperty();

    MidiFunctionBlockProperty BuildFunctionBlockPropertyFromInfoNotificationMessage(_In_ internal::PackedUmp128& ump);

    HRESULT RequestAllFunctionBlocks();
    HRESULT RequestAllEndpointDiscoveryInformation();

    HRESULT ProcessDeviceIdentityNotificationMessage(_In_ internal::PackedUmp128& ump);
    HRESULT ProcessEndpointInfoNotificationMessage(_In_ internal::PackedUmp128& ump);
    HRESULT ProcessEndpointNameNotificationMessage(_In_ internal::PackedUmp128& ump);
    HRESULT ProcessProductInstanceIdNotificationMessage(_In_ internal::PackedUmp128& ump);
    HRESULT ProcessFunctionBlockInfoNotificationMessage(_In_ internal::PackedUmp128& ump);
    HRESULT ProcessFunctionBlockNameNotificationMessage(_In_ internal::PackedUmp128& ump);
    HRESULT ProcessStreamConfigurationRequest(_In_ internal::PackedUmp128& ump);


    std::wstring ParseStreamTextMessage(_In_ internal::PackedUmp128& message);

    DEVPROPKEY FunctionBlockPropertyKeyFromNumber(_In_ uint8_t functionBlockNumber);
    DEVPROPKEY FunctionBlockNamePropertyKeyFromNumber(_In_ uint8_t functionBlockNumber);

    HRESULT UpdateEndpointNameProperty();
    HRESULT UpdateEndpointProductInstanceIdProperty();
    HRESULT UpdateFunctionBlockNameProperty(_In_ uint8_t functionBlockNumber, _In_ std::wstring value);

    HRESULT UpdateDeviceIdentityProperty(_In_ internal::PackedUmp128& identityMessage);
    HRESULT UpdateEndpointInfoProperties(_In_ internal::PackedUmp128& endpointInfoNotificationMessage);
    HRESULT UpdateStreamConfigurationProperties(_In_ internal::PackedUmp128& endpointStreamConfigurationNotificationMessage);

    HRESULT UpdateFunctionBlockProperty(_In_ internal::PackedUmp128& functionBlockInfoNotificationMessage);
    HRESULT UpdateAllFunctionBlockPropertiesIfComplete();

};


