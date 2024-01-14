// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class CMidiEndpointProtocolNegotiationWorker : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiCallback>
{
public:
    HRESULT Start(
        _In_ LPCWSTR DeviceInterfaceId,
        _In_ BOOL PreferToSendJRTimestampsToEndpoint,
        _In_ BOOL PreferToReceiveJRTimestampsFromEndpoint,
        _In_ BYTE PreferredMidiProtocol,
        _In_ WORD TimeoutMS,
        _In_ wil::com_ptr_nothrow<IMidiAbstraction> ServiceAbstraction);

    STDMETHOD(Callback)(_In_ PVOID Data, _In_ UINT Size, _In_ LONGLONG Position, _In_ LONGLONG Context);

private:
    HRESULT RequestAllFunctionBlocks();
    HRESULT RequestAllEndpointDiscoveryInformation();
    HRESULT ProcessStreamConfigurationRequest(_In_ internal::PackedUmp128 ump);

    wil::com_ptr_nothrow<IMidiBiDi> m_endpoint;

    bool m_preferToSendJRTimestampsToEndpoint{ false };
    bool m_preferToReceiveJRTimestampsFromEndpoint{ false };
    uint8_t m_preferredMidiProtocol{  };

    bool m_alreadyTriedToNegotiationOnce{ false };

    wil::unique_event_nothrow m_allMessagesReceived;
    bool m_endpointInfoReceived{ false };
    bool m_finalStreamNegotiationResponseReceived{ false };
    bool m_endpointNameReceived{ false };
    bool m_endpointProductInstanceIdReceived{ false };
    bool m_deviceIdentityReceived{ false };

    uint8_t m_declaredFunctionBlockCount{ 0 };

    uint8_t m_countFunctionBlocksReceived{ 0 };
    uint8_t m_countFunctionBlockNamesReceived{ 0 };

};


class CMidiEndpointProtocolManager : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiEndpointProtocolManagerInterface>
{
public:

    CMidiEndpointProtocolManager() = default;
    ~CMidiEndpointProtocolManager() {}

    HRESULT Initialize(
        _In_ std::shared_ptr<CMidiClientManager>& ClientManager,
        _In_ std::shared_ptr<CMidiDeviceManager>& DeviceManager
    );

    STDMETHOD(NegotiateAndRequestMetadata)(
            _In_ LPCWSTR DeviceInterfaceId,
            _In_ BOOL PreferToSendJRTimestampsToEndpoint,
            _In_ BOOL PreferToReceiveJRTimestampsFromEndpoint,
            _In_ BYTE PreferredMidiProtocol,
            _In_ WORD TimeoutMS
        );

    HRESULT Cleanup();

private:
    std::shared_ptr<CMidiClientManager> m_clientManager;
    std::shared_ptr<CMidiDeviceManager> m_deviceManager;

    wil::com_ptr_nothrow<IMidiAbstraction> m_serviceAbstraction;


};
