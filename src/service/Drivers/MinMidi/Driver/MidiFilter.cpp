// Copyright (c) Microsoft Corporation. All rights reserved.
#include "common.h"
#include "MidiPin.h"
#include "MidiFilter.h"

static const
GUID g_Categories[] =
{
    STATICGUIDOF(KSCATEGORY_AUDIO)
};

static const
KSFILTER_DISPATCH g_FilterDispatch =
{
    MidiFilter::Create,
    MidiFilter::Close,
    nullptr, // Process
    nullptr // Reset
};

static const
KSTOPOLOGY_CONNECTION g_FilterConnections[] =
{
    {KSFILTER_NODE, 0, KSFILTER_NODE, 1},
    {KSFILTER_NODE, 3, KSFILTER_NODE, 2}
};

DEFINE_KSFILTER_DESCRIPTOR (g_MidiFilterDescriptor)
{
    &g_FilterDispatch,
    nullptr, // AutomationTable
    KSFILTER_DESCRIPTOR_VERSION,
    KSFILTER_FLAG_CRITICAL_PROCESSING,
    &KSNAME_Filter, // ReferenceGuid
    DEFINE_KSFILTER_PIN_DESCRIPTORS (g_MidiPinDescriptors),
    DEFINE_KSFILTER_CATEGORIES (g_Categories),
    DEFINE_KSFILTER_NODE_DESCRIPTORS_NULL,
    DEFINE_KSFILTER_CONNECTIONS (g_FilterConnections),
    nullptr // ComponentId
};

_Use_decl_annotations_
MidiFilter::MidiFilter(
    PKSFILTER Filter
) : m_Filter(Filter)
{
    PAGED_CODE();
}

_Use_decl_annotations_
MidiFilter::~MidiFilter()
{
    PAGED_CODE();
}

_Use_decl_annotations_
NTSTATUS
MidiFilter::Create(
    PKSFILTER Filter,
    PIRP /*irp*/
)
{
    PAGED_CODE();

    MidiFilter* midiFilter = new (POOL_FLAG_NON_PAGED) MidiFilter(Filter);

    NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !midiFilter);

    Filter->Context = (PVOID)midiFilter;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MidiFilter::Close(
    PKSFILTER Filter,
    PIRP /* Irp */
)
{
    PAGED_CODE();

    MidiFilter * midiFilter = (MidiFilter *)Filter->Context;
    if (midiFilter)
    {
        delete midiFilter;
        Filter->Context = nullptr;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
CreateFilterFactory(
    PKSDEVICE device
)
{
    PAGED_CODE();

    PKSFILTERFACTORY pKsFilterFactory = nullptr;

    NT_RETURN_IF_NTSTATUS_FAILED(KsCreateFilterFactory( device->FunctionalDeviceObject,
                                      &g_MidiFilterDescriptor,
                                      L"MinMidi",
                                      nullptr,
                                      KSCREATE_ITEM_FREEONSTOP,
                                      nullptr, // Sleep Callback
                                      nullptr, // Wake Callback
                                      &pKsFilterFactory));

    return STATUS_SUCCESS;
}

