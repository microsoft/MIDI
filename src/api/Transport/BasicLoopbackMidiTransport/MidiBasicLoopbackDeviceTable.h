// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once


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

    std::shared_ptr<MidiBasicLoopbackDevice> GetDevice(_In_ winrt::guid const& associationId);
    std::shared_ptr<MidiBasicLoopbackDevice> GetDeviceById(_In_ std::wstring const& endpointDeviceId);


    void SetDevice(_In_ winrt::guid const& associationId, _In_ std::shared_ptr<MidiBasicLoopbackDevice> device);
    void RemoveDevice(_In_ winrt::guid const& associationId);

    bool IsUniqueIdentifierInUseForLoopback(_In_ std::wstring const& uniqueIdentifier);

    HRESULT Shutdown();


};
