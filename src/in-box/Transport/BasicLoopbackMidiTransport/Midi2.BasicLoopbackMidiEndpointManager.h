// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once


class CMidi2BasicLoopbackMidiEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>

{
public:
    STDMETHOD(Initialize(_In_ IMidiDeviceManager*, _In_ IMidiEndpointProtocolManager*));
    STDMETHOD(Shutdown)();

    // The caller has reserved the definition's association id and unique id in the endpoint table.
    // Success hands the reservation over to the published device; failure releases it.
    HRESULT CreateEndpoint(
        _In_ std::shared_ptr<MidiBasicLoopbackDeviceDefinition> definition
    );

    HRESULT DeleteEndpoint(
        _In_ std::shared_ptr<MidiBasicLoopbackDeviceDefinition const> definition
    );

    HRESULT UpdateEndpointMutedStateProperty(
        _In_ std::shared_ptr<MidiBasicLoopbackDeviceDefinition const> definition
    );

    bool IsInitialized() const noexcept { return m_initialized.load(); }

    // Creates the loopbacks queued before the endpoint manager was ready. Safe to call from more
    // than one thread at once: each queued loopback is taken by exactly one caller.
    HRESULT ProcessWorkQueue();

private:
    HRESULT CreateParentDevice();

    // Copied out under the lock, because the configuration manager and feedback trips call in on
    // their own threads while Shutdown may be letting go of it.
    wil::com_ptr_nothrow<IMidiDeviceManager> GetDeviceManager() const
    {
        auto lock = m_serviceLock.lock_shared();
        return m_MidiDeviceManager;
    }

    // Set only once Initialize has fully succeeded, and read from other threads
    std::atomic<bool> m_initialized{ false };

    GUID m_TransportTransportId{};

    std::wstring m_parentDeviceId{};

    mutable wil::srwlock m_serviceLock;
    wil::com_ptr_nothrow<IMidiDeviceManager> m_MidiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> m_MidiProtocolManager;
};
