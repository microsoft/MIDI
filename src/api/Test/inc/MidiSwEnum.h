// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

typedef class _MIDIU_DEVICE
{
public:
    GUID AbstractionLayer;
    GUID InterfaceClass;
    std::wstring InstanceId;
    std::wstring Name; // friendly name for this device
    MidiFlow Flow{ MidiFlowOut };
    BOOL MidiOne{ FALSE };
} MIDIU_DEVICE, *PMIDIU_DEVICE;

class MidiSWDeviceEnum
{
public:
    MidiSWDeviceEnum();
    virtual ~MidiSWDeviceEnum();
    HRESULT EnumerateDevices();
    virtual HRESULT Cleanup();

    UINT GetNumMidiDevices(_In_ MidiFlow, _In_ BOOL MidiOne=FALSE);
    std::wstring GetMidiInstanceId(_In_ UINT, _In_ MidiFlow, _In_ BOOL MidiOne=FALSE);

private:
    std::vector<std::unique_ptr<MIDIU_DEVICE>> m_AvailableMidiDevices;
};
