// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

extern const KSFILTER_DESCRIPTOR g_MidiFilterDescriptorBytestream;
extern const KSFILTER_DESCRIPTOR g_MidiFilterDescriptorUMP;

#define MAX_FILTERFACTORIES 20

typedef struct _FILTER_INSTANCE
{
    KSFILTER_DESCRIPTOR FilterDescriptor {0};
    const WCHAR *ReferenceString {nullptr};
    wil::fast_mutex_with_critical_region MidiPinLock;
    MidiPin *MidiInPin {nullptr};
    MidiPin *MidiOutPin {nullptr};
    PKSFILTERFACTORY FilterFactory {nullptr};
} FILTER_INSTANCE, *PFILTER_INSTANCE;

class MidiPin;

PAGED_CODE_SEG
NTSTATUS
CreateFilterFactory(
    _In_ PKSDEVICE device,
    _In_ PFILTER_INSTANCE filterInstance
);

class MidiFilter
{
    friend class MidiPin;

public:

    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    MidiFilter(
        _In_ PKSFILTER filter,
        _In_ PFILTER_INSTANCE filterInstance
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
    PFILTER_INSTANCE m_FilterInstance {nullptr};
};
