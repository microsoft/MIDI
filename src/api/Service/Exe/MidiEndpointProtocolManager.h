// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

struct ProtocolNegotiationWorkerThreadEntry
{
    std::shared_ptr<std::thread> Thread;
    std::shared_ptr<CMidiEndpointProtocolWorker> Worker;
};

class CMidiEndpointProtocolManager : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiEndpointProtocolManagerInterface>
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
        _In_ GUID AbstractionGuid,
        _In_ LPCWSTR DeviceInterfaceId,
        _In_ ENDPOINTPROTOCOLNEGOTIATIONPARAMS NegotiationParams,
        _In_ IMidiProtocolNegotiationCompleteCallback* NegotiationCompleteCallback
        );


    HRESULT Cleanup();

private:
    GUID m_sessionId;

    std::shared_ptr<CMidiClientManager> m_clientManager;
    std::shared_ptr<CMidiDeviceManager> m_deviceManager;
    std::shared_ptr<CMidiSessionTracker> m_sessionTracker;

    std::map<std::wstring, ProtocolNegotiationWorkerThreadEntry> m_endpointWorkers;

};
