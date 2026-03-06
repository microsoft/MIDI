// Copyright (c) Microsoft Corporation. All rights reserved.
#include "common.h"
#include "MidiPin.h"
#include "MidiFilter.h"
#include "MidiDev.h"

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

DEFINE_KSFILTER_DESCRIPTOR (g_MidiFilterDescriptorBytestream)
{
    &g_FilterDispatch,
    nullptr, // AutomationTable
    KSFILTER_DESCRIPTOR_VERSION,
    KSFILTER_FLAG_CRITICAL_PROCESSING,
    &KSNAME_Filter, // ReferenceGuid
    DEFINE_KSFILTER_PIN_DESCRIPTORS (g_MidiPinDescriptorsBytestream),
    DEFINE_KSFILTER_CATEGORIES (g_Categories),
    DEFINE_KSFILTER_NODE_DESCRIPTORS_NULL,
    DEFINE_KSFILTER_CONNECTIONS (g_FilterConnections),
    nullptr // ComponentId
};

DEFINE_KSFILTER_DESCRIPTOR (g_MidiFilterDescriptorUMP)
{
    &g_FilterDispatch,
    nullptr, // AutomationTable
    KSFILTER_DESCRIPTOR_VERSION,
    KSFILTER_FLAG_CRITICAL_PROCESSING,
    &KSNAME_Filter, // ReferenceGuid
    DEFINE_KSFILTER_PIN_DESCRIPTORS (g_MidiPinDescriptorsUMP),
    DEFINE_KSFILTER_CATEGORIES (g_Categories),
    DEFINE_KSFILTER_NODE_DESCRIPTORS_NULL,
    DEFINE_KSFILTER_CONNECTIONS (g_FilterConnections),
    nullptr // ComponentId
};

_Use_decl_annotations_
MidiFilter::MidiFilter(
    PKSFILTER filter,
    PFILTER_INSTANCE filterInstance
) : m_Filter(filter),
    m_FilterInstance(filterInstance)
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
    PKSFILTER filter,
    PIRP /*irp*/
)
{
    PAGED_CODE();

    KsFilterAcquireControl(filter);
    auto releaseDeviceOnExit = wil::scope_exit([filter]() { KsFilterReleaseControl(filter); });

    // Filter instance information is passed through the factory context, stored
    // on the device
    PKSFILTERFACTORY filterFactory = KsFilterGetParentFilterFactory(filter);

    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, !filterFactory);

    PFILTER_INSTANCE filterInstance = (FILTER_INSTANCE*) filterFactory->Context;

    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, !filterInstance);

    MidiFilter* midiFilter = new (POOL_FLAG_NON_PAGED) MidiFilter(filter, filterInstance);

    NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !midiFilter);

    filter->Context = (PVOID)midiFilter;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MidiFilter::Close(
    PKSFILTER filter,
    PIRP irp
)
{
    PAGED_CODE();

    KsFilterAcquireControl(filter);
    auto releaseDeviceOnExit = wil::scope_exit([filter]() { KsFilterReleaseControl(filter); });

    MidiFilter * midiFilter = (MidiFilter *)filter->Context;
    if (midiFilter)
    {
        for (UINT i = 0; i < filter->Descriptor->PinDescriptorsCount; i++)
        {
            PKSPIN pin = (PKSPIN) KsFilterGetFirstChildPin(filter, i);
            while (pin)
            {
                PKSPIN nextPin = KsPinGetNextSiblingPin(pin);
            
                // Get the MidiFilter context and clean it up
                MidiPin* midiPin = (MidiPin*)pin->Context;
                if (midiPin)
                {
                    midiPin->Close(pin, irp);
                }

                pin = nextPin;
            }
        }

        delete midiFilter;
        filter->Context = nullptr;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
CreateFilterFactory(
    PKSDEVICE device,
    PFILTER_INSTANCE filterInstance
)
{
    PAGED_CODE();

    NT_RETURN_IF_NTSTATUS_FAILED(KsCreateFilterFactory( device->FunctionalDeviceObject,
                                      &(filterInstance->FilterDescriptor),
                                      (PWSTR) filterInstance->ReferenceString,
                                      nullptr,
                                      KSCREATE_ITEM_FREEONSTOP,
                                      nullptr, // Sleep Callback
                                      nullptr, // Wake Callback
                                      &(filterInstance->FilterFactory)));

    filterInstance->FilterFactory->Context = (PVOID) filterInstance;

    return STATUS_SUCCESS;
}

