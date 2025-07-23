// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class CMidi2LoopbackMidiBidi :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiBidirectional,
    IMidiCallback>
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ PTRANSPORTCREATIONPARAMS, _In_ DWORD*, _In_opt_ IMidiCallback*, _In_ LONGLONG, _In_ GUID));
    STDMETHOD(SendMidiMessage(_In_ MessageOptionFlags, _In_ PVOID message, _In_ UINT size, _In_ LONGLONG));
    STDMETHOD(Callback)(_In_ MessageOptionFlags, _In_ PVOID, _In_ UINT, _In_ LONGLONG, _In_ LONGLONG);
    STDMETHOD(Shutdown)();

    void AssociationId(_In_ std::wstring newId) { m_associationId = newId; }

    //void LinkAssociatedBidi(_In_ wil::com_ptr_nothrow<CMidi2LoopbackMidiBidi> associatedBidi) 
    //{ 
    //    m_associatedBidi = associatedBidi; 

    //    LOG_IF_FAILED(associatedBidi->QueryInterface(__uuidof(IMidiCallback), (void**)&m_associatedBidiCallback));
    //}

    //void UnlinkAssociatedBidi()
    //{
    //    m_associatedBidi = nullptr;
    //    m_associatedBidiCallback = nullptr;
    //}

private:
    bool m_isEndpointA = false;

    std::wstring m_associationId{};
    //wil::com_ptr_nothrow<CMidi2LoopbackMidiBidi> m_associatedBidi;
    //wil::com_ptr_nothrow<IMidiCallback> m_associatedBidiCallback;


    wil::com_ptr_nothrow <IMidiCallback> m_callback{ nullptr };
    LONGLONG m_callbackContext{};

    std::wstring m_endpointId{};
};


