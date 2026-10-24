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
    ~CMidi2MidiSynthEndpointManager();

    STDMETHOD(Initialize(_In_ IMidiDeviceManager*, _In_ IMidiEndpointProtocolManager*));
    STDMETHOD(Shutdown)();

    bool IsInitialized() const noexcept { return m_initialized.load(); }

    // Creates the endpoint when the synthesizer is switched on and removes it when it is switched
    // off. Removing it is the point of the off switch: an application which opens every MIDI port
    // it can see cannot hold open an endpoint that does not exist, and so cannot keep the audio
    // device away from an application that wants it in WASAPI exclusive mode, or through ASIO.
    HRESULT SyncEndpointToSettings();

    // Waits for the negotiation thread and lets go of the service's objects. Idempotent. Called by
    // TransportState::Shutdown, so it runs whichever manager the service shuts down first, and by
    // a failed Initialize, which the service never follows with a Shutdown.
    void ReleaseServiceObjects() noexcept;

    std::wstring EndpointDeviceInterfaceId() const
    {
        auto lock = m_endpointLock.lock_shared();
        return m_endpointDeviceInterfaceId;
    }

private:
    HRESULT CreateParentDevice();
    HRESULT CreateEndpoint();
    HRESULT RemoveEndpoint();
    HRESULT InitiateDiscoveryAndNegotiation(_In_ std::wstring const& endpointInterfaceId);

    // Caller must hold m_syncLock.
    HRESULT SyncEndpointToSettingsLocked();

    // Read without the sync lock by a settings change deciding whether to sync at all. Set only
    // once Initialize has fully succeeded.
    std::atomic<bool> m_initialized{ false };

    // Held for a whole sync, and by Shutdown. Two settings changes arriving together could otherwise
    // both create the endpoint, or both start a negotiation thread and assign one over the other,
    // which ends the service. Taken after the device's settings change lock and before any of the
    // device's other locks.
    wil::critical_section m_syncLock;

    GUID m_transportId{};

    std::wstring m_parentDeviceId{};

    // Guards the ids, so they can be read without waiting for a sync to finish.
    mutable wil::srwlock m_endpointLock;
    std::wstring m_endpointShortInstanceId{};
    std::wstring m_endpointDeviceInterfaceId{};

    // Guarded by m_syncLock: only Initialize, a sync and ReleaseServiceObjects use them.
    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> m_midiProtocolManager;

    // Discovery opens a client connection back to this endpoint, so it cannot run on the thread
    // that is still creating it. Guarded by m_syncLock.
    std::thread m_negotiationThread;
};
