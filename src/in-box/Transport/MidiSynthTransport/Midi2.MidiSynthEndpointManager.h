// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class CMidi2MidiSynthEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>
{
public:
    STDMETHOD(Initialize(_In_ IMidiDeviceManager*, _In_ IMidiEndpointProtocolManager*));
    STDMETHOD(Shutdown)();

    bool IsInitialized() const noexcept { return m_initialized; }

    // Creates the endpoint when the synthesizer is switched on and removes it when it is switched
    // off. Removing it is the point of the off switch: an application which opens every MIDI port
    // it can see cannot hold open an endpoint that does not exist, and so cannot keep the audio
    // device away from an exclusive-mode or ASIO application.
    HRESULT SyncEndpointToSettings();

    std::wstring EndpointDeviceInterfaceId() const
    {
        auto lock = m_endpointLock.lock_shared();
        return m_endpointDeviceInterfaceId;
    }

private:
    HRESULT CreateParentDevice();
    HRESULT CreateEndpoint();
    HRESULT RemoveEndpoint();
    HRESULT WriteDeviceIdentity(_In_ std::wstring const& endpointInterfaceId);

    bool m_initialized{ false };

    GUID m_containerId{};
    GUID m_transportId{};

    std::wstring m_parentDeviceId{};

    // Guards endpoint creation and removal against two settings changes arriving at once.
    mutable wil::srwlock m_endpointLock;
    std::wstring m_endpointShortInstanceId{};
    std::wstring m_endpointDeviceInterfaceId{};

    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> m_midiProtocolManager;
};
