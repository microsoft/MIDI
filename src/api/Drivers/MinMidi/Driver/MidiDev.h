// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

extern "C" DRIVER_INITIALIZE DriverEntry;

class MidiDevice
{
public:

    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    MidiDevice(
        _In_ PKSDEVICE Device
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
        _In_ PKSDEVICE  Device
    );

    _Must_inspect_result_
    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    static
    NTSTATUS
    Start(
        _In_ PKSDEVICE Device,
        _In_ PIRP Irp,
        _In_opt_ PCM_RESOURCE_LIST TranslatedResourceList,
        _In_opt_ PCM_RESOURCE_LIST UntranslatedResourceList
    );

    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    static
    void 
    Stop(
        _In_ PKSDEVICE Device,
        _In_ PIRP Irp
    );

    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    static
    void
    Remove(
        _In_ PKSDEVICE Device,
        _In_ PIRP Irp
    );

    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    static
    void
    SurpriseRemoval(
        _In_ PKSDEVICE Device,
        _In_ PIRP Irp
    );

private:
    PKSDEVICE m_Device;
};
