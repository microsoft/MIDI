// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once



// TODO: This class can implement another interface which takes in the json parameters 
// (or whatever we want) for naming the new endpoints. Then, the SDK can call that
// interface to do the setup of the device. Or, there can be a common "runtime creatable transport"
// interface that is called from the API. TBD. But can't break that interface with 
// the SDK, since the SDK ships with apps and could be older.


class CMidi2VirtualPatchBayEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>

{
public:
    STDMETHOD(Initialize(_In_ IUnknown*, _In_ IUnknown*));
    STDMETHOD(Cleanup)();


private:
    GUID m_ContainerId{};
    GUID m_TransportAbstractionId{};

    HRESULT CreateEndpoint(
    );

    //HRESULT CreateParentDevice();

    wil::com_ptr_nothrow<IMidiDeviceManagerInterface> m_MidiDeviceManager;

};
