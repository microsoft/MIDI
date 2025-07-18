// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

typedef class _MIDIU_DEVICE
{
public:
    GUID TransportLayer;
    GUID InterfaceClass;
    std::wstring DeviceId;
    std::wstring DeviceInstanceId;
    std::wstring ParentDeviceInstanceId;
    std::wstring Name; // friendly name for this device
    MidiFlow Flow{ MidiFlowOut };
    MidiDataFormats SupportedDataFormats{ MidiDataFormats_Invalid };
    BOOL MidiOne{ FALSE };
} MIDIU_DEVICE, *PMIDIU_DEVICE;

class MidiSWDeviceEnum
{
public:
    static HRESULT EnumerateDevices(_In_ std::vector<std::unique_ptr<MIDIU_DEVICE>>&, _In_ std::function<bool(PMIDIU_DEVICE)>&&);
};
