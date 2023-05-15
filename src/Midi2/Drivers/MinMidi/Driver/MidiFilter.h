// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

extern const KSFILTER_DESCRIPTOR g_MidiFilterDescriptor;

class MidiPin;

NTSTATUS
CreateFilterFactory(
    _In_ PKSDEVICE device
);

class MidiFilter
{
    friend class MidiPin;

public:

    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    MidiFilter(
        _In_ PKSFILTER Filter
    );

    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    virtual
    ~MidiFilter();

    _Must_inspect_result_
    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    static
    NTSTATUS
    Create(
        _Inout_ PKSFILTER filter,
        _In_ PIRP irp
    );

    _Must_inspect_result_
    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    static
    NTSTATUS
    Close(
        _In_ PKSFILTER filter,
        _In_ PIRP irp
    );

private:
    PKSFILTER m_Filter {nullptr};
};
