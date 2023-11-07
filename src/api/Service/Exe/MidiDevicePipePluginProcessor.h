// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

// this is only a COM object so it can implement IMidiBiDi
class CMidiDevicePipePluginBookend : 
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiBiDi>
{
public:


    HRESULT SendMidiMessage(_In_ PVOID, _In_ UINT, _In_ LONGLONG);

    STDMETHOD(Callback)(_In_ PVOID, _In_ UINT, _In_ LONGLONG);

private:


};


class CMidiDevicePipePluginProcessor :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiCallback>
{
public:



    HRESULT SendMidiMessage(_In_ PVOID, _In_ UINT, _In_ LONGLONG);

    STDMETHOD(Callback)(_In_ PVOID, _In_ UINT, _In_ LONGLONG);

private:
    // these may need to be interfaces. TBD.
    wil::com_ptr_nothrow<CMidiDevicePipePluginBookend> m_deviceSideBookend;
    wil::com_ptr_nothrow<CMidiDevicePipePluginBookend> m_clientSideBookend;

};



