// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

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

    STDMETHOD(
        NegotiateAndRequestMetadata)(
            _In_ LPCWSTR DeviceInterfaceId,
            _In_ BOOL PreferToSendJRTimestampsToEndpoint,
            _In_ BOOL PreferToReceiveJRTimestampsFromEndpoint,
            _In_ BYTE PreferredMidiProtocol,
            _In_ WORD TimeoutMS
        );


    HRESULT Cleanup();

private:


};
