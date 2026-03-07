// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

extern "C" DRIVER_INITIALIZE DriverEntry;

typedef enum
{
    MidiDataFormats_Invalid = 0,
    MidiDataFormats_ByteStream = 0x1,
    MidiDataFormats_UMP = 0x2,
    MidiDataFormats_Any = 0xFFFFFFFF
} MidiDataFormats;

class MidiDevice
{
public:

    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    MidiDevice(
        _In_ PKSDEVICE device
    );

    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    virtual
    ~MidiDevice ();

    _Must_inspect_result_
    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    static
    NTSTATUS
    Create(
        _In_ PKSDEVICE  device
    );

    _Must_inspect_result_
    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    static
    NTSTATUS
    Start(
        _In_ PKSDEVICE device,
        _In_ PIRP irp,
        _In_opt_ PCM_RESOURCE_LIST translatedResourceList,
        _In_opt_ PCM_RESOURCE_LIST untranslatedResourceList
    );

    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    static
    void 
    Stop(
        _In_ PKSDEVICE device,
        _In_ PIRP irp
    );

    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    static
    void
    Remove(
        _In_ PKSDEVICE device,
        _In_ PIRP irp
    );

    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    static
    void
    SurpriseRemoval(
        _In_ PKSDEVICE device,
        _In_ PIRP irp
    );

    NTSTATUS
    MidiDevice::AddPort
    (
        _In_ PKSDEVICE device,
        _In_ MidiDataFormats formats
    );

    NTSTATUS
    MidiDevice::RemovePort
    (
        _In_ MidiDataFormats formats
    );

private:
    PKSDEVICE m_Device {nullptr};
    UINT m_NumBytestreamFilterInstancesUsed {0};
    FILTER_INSTANCE m_BytestreamFilterInstance[MAX_FILTERFACTORIES];

    UINT m_NumUMPFilterInstancesUsed {0};
    FILTER_INSTANCE m_UMPFilterInstance[MAX_FILTERFACTORIES];
};
