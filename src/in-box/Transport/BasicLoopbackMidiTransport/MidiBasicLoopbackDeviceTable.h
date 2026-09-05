// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once


// What listEntries reports about one loopback. The definition is the stored configuration; the
// message count is live and belongs to the device rather than to its definition, so the two are
// gathered together here instead of being folded into the definition itself.
struct MidiBasicLoopbackDeviceSnapshot
{
    MidiBasicLoopbackDeviceDefinition Definition{};

    uint64_t MessageCount{ 0 };
};


class MidiBasicLoopbackDeviceTable
{
private:
    // unlike GUID, winrt::guid has built-in comparison so it can be used as a key in std::map
    std::map<winrt::guid, std::shared_ptr<MidiBasicLoopbackDevice>> m_devices;

    // guards all access to m_devices. Accessed from the data path
    // (GetDeviceById via SendMidiMessage), the activation path (SetDevice),
    // and teardown (RemoveDevice / Shutdown), so it must be synchronized.
    wil::srwlock m_devicesLock;


public:
    std::vector<MidiBasicLoopbackDeviceSnapshot> GetDeviceListSnapshot();

    std::shared_ptr<MidiBasicLoopbackDevice> GetDevice(_In_ winrt::guid const& associationId);
    std::shared_ptr<MidiBasicLoopbackDevice> GetDeviceById(_In_ std::wstring const& endpointDeviceId);


    void SetDevice(_In_ winrt::guid const& associationId, _In_ std::shared_ptr<MidiBasicLoopbackDevice> device);
    void RemoveDevice(_In_ winrt::guid const& associationId);

    bool IsUniqueIdentifierInUseForLoopback(_In_ std::wstring const& uniqueIdentifier);

    HRESULT Shutdown();


};
