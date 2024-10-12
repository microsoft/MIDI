// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class CMidi2LoopbackMidiBiDi :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiBiDi,
    IMidiCallback>
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ PTRANSPORTCREATIONPARAMS, _In_ DWORD*, _In_opt_ IMidiCallback*, _In_ LONGLONG, _In_ GUID));
    STDMETHOD(SendMidiMessage(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG));
    STDMETHOD(Callback)(_In_ PVOID, _In_ UINT, _In_ LONGLONG, _In_ LONGLONG);
    STDMETHOD(Shutdown)();

    void AssociationId(_In_ std::wstring newId) { m_associationId = newId; }

    //void LinkAssociatedBiDi(_In_ wil::com_ptr_nothrow<CMidi2LoopbackMidiBiDi> associatedBiDi) 
    //{ 
    //    m_associatedBiDi = associatedBiDi; 

    //    LOG_IF_FAILED(associatedBiDi->QueryInterface(__uuidof(IMidiCallback), (void**)&m_associatedBiDiCallback));
    //}

    //void UnlinkAssociatedBiDi()
    //{
    //    m_associatedBiDi = nullptr;
    //    m_associatedBiDiCallback = nullptr;
    //}

private:
    bool m_isEndpointA = false;

    std::wstring m_associationId{};
    //wil::com_ptr_nothrow<CMidi2LoopbackMidiBiDi> m_associatedBiDi;
    //wil::com_ptr_nothrow<IMidiCallback> m_associatedBiDiCallback;


    wil::com_ptr_nothrow <IMidiCallback> m_callback{ nullptr };
    LONGLONG m_callbackContext{};

    std::wstring m_endpointId{};
};


