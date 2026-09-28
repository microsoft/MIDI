// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// The service's view of one rtpMIDI connection's endpoint.
// ============================================================================

#pragma once

class CMidi2RtpMidiBidi :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiBidirectional,
        IMidiCallback>
{
public:
    STDMETHOD(Initialize)(_In_ LPCWSTR endpointDeviceInterfaceId, _In_ PTRANSPORTCREATIONPARAMS creationParams, _In_ DWORD* mmCssTaskId, _In_opt_ IMidiCallback* callback, _In_ LONGLONG context, _In_ GUID sessionId);
    STDMETHOD(SendMidiMessage)(_In_ MessageOptionFlags optionFlags, _In_ PVOID message, _In_ UINT size, _In_ LONGLONG position);
    STDMETHOD(Callback)(_In_ MessageOptionFlags optionFlags, _In_ PVOID message, _In_ UINT size, _In_ LONGLONG position, _In_ LONGLONG context);
    STDMETHOD(Shutdown)();

private:
    wil::com_ptr_nothrow<IMidiCallback> m_callback{ nullptr };
    std::weak_ptr<RtpMidiConnection> m_connection;
};
