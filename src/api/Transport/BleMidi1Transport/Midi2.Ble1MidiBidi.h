// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class CMidi2Ble1MidiBidi : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiBidirectional,
        IMidiCallback>
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ PTRANSPORTCREATIONPARAMS, _In_ DWORD *, _In_opt_ IMidiCallback *, _In_ LONGLONG, _In_ GUID));
    STDMETHOD(SendMidiMessage(_In_ MessageOptionFlags optionFlags, _In_ PVOID message, _In_ UINT size, _In_ LONGLONG));
    STDMETHOD(Callback)(_In_ MessageOptionFlags optionFlags, _In_ PVOID, _In_ UINT, _In_ LONGLONG, _In_ LONGLONG);
    STDMETHOD(Shutdown)();

private:
    wil::com_ptr_nothrow<IMidiCallback> m_callback{ nullptr };
    LONGLONG m_context{ 0 };

    std::wstring m_endpointDeviceInterfaceId{ };

    //std::weak_ptr<MidiNetworkConnection> m_connection;
};


