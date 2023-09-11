// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

typedef class _MIDIU_DEVICE
{
public:
    GUID AbstractionLayer;
    GUID InterfaceClass;
    std::wstring DeviceId;
    std::wstring DeviceInstanceId;
    std::wstring ParentDeviceInstanceId;
    std::wstring Name; // friendly name for this device
    MidiFlow Flow{ MidiFlowOut };
    BOOL MidiOne{ FALSE };
} MIDIU_DEVICE, *PMIDIU_DEVICE;

class MidiSWDeviceEnum
{
public:
    static HRESULT EnumerateDevices(std::vector<std::unique_ptr<MIDIU_DEVICE>>& Devices, std::function<bool(PMIDIU_DEVICE)>&& Predicate);
};
