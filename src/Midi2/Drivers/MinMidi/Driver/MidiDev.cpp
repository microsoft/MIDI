// Copyright (c) Microsoft Corporation. All rights reserved.
#include "common.h"
#include "MidiDev.h"
#include "MidiFilter.h"

extern wil::fast_mutex_with_critical_region *g_MidiInLock;

static const
KSDEVICE_DISPATCH DeviceDispatch = {
    MidiDevice::Create,
    MidiDevice::Start,
    NULL, // PostStart
    NULL, // QueryStop
    NULL, // CancelStop
    MidiDevice::Stop,
    NULL, // QueryRemove
    NULL, // CancelRemove
    MidiDevice::Remove,
    NULL, // QueryCapabilities
    MidiDevice::SurpriseRemoval,
    NULL, // QueryPower
    NULL, // SetPower
    NULL  // QueryInterface
};

DEFINE_KSFILTER_DESCRIPTOR_TABLE(g_MidiFilterDescriptors) {
    &g_MidiFilterDescriptor
};

static const
KSDEVICE_DESCRIPTOR DeviceDescriptor =
{
    &DeviceDispatch,
    0, // SIZEOF_ARRAY(g_MidiFilterDescriptors),
    NULL, //g_MidiFilterDescriptors,
    KSDEVICE_DESCRIPTOR_VERSION
};

DRIVER_UNLOAD DriverUnload;

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING pRegistryPathName
)
{
    PAGED_CODE();
    NT_RETURN_IF_NTSTATUS_FAILED(KsInitializeDriver(DriverObject, pRegistryPathName, &DeviceDescriptor));

    DriverObject->DriverUnload = DriverUnload;

    if (!g_MidiInLock)
    {
        g_MidiInLock = new (POOL_FLAG_NON_PAGED) wil::fast_mutex_with_critical_region();
    }

    return STATUS_SUCCESS;
}

void
DriverUnload( 
    _In_ PDRIVER_OBJECT /* DriverObject */
)
{
    PAGED_CODE();

    if (g_MidiInLock)
    {
        delete g_MidiInLock;
        g_MidiInLock = nullptr;
    }
}

_Use_decl_annotations_
MidiDevice::MidiDevice (
    PKSDEVICE Device
) : m_Device(Device)
{
    PAGED_CODE();
}

_Use_decl_annotations_
MidiDevice::~MidiDevice ()
{
    PAGED_CODE();
}

_Use_decl_annotations_
NTSTATUS
MidiDevice::Create(
    PKSDEVICE Device
)
{
    PAGED_CODE ();

    MidiDevice *midiDevice = new (POOL_FLAG_NON_PAGED) MidiDevice(Device);

    NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !midiDevice);

    Device->Context = midiDevice;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MidiDevice::Start(
    PKSDEVICE Device,
    PIRP /* Irp */,
    PCM_RESOURCE_LIST /* TranslatedResourceList */,
    PCM_RESOURCE_LIST /* UntranslatedResourceList */
)
{
    PAGED_CODE();

    // Create the filter factory. It is done this way, rather than using the KSDEVICE_DESCRIPTOR
    // so that a reference string can be provided for the name of the filter. Using the KSDEVICE_DESCRIPTOR,
    // only a reference guid can be used.
    NT_RETURN_IF_NTSTATUS_FAILED(CreateFilterFactory(Device));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void 
MidiDevice::Stop(
    PKSDEVICE /* Device */,
    PIRP /* Irp */
)
{
    PAGED_CODE ();
}

_Use_decl_annotations_
void
MidiDevice::Remove(
    PKSDEVICE Device,
    PIRP Irp
)
{
    PAGED_CODE ();

    MidiDevice::Stop(Device, Irp);

    MidiDevice * midiDevice = (MidiDevice *)Device->Context;
    if (midiDevice)
    {
        delete midiDevice;
        Device->Context = nullptr;
    }
}

_Use_decl_annotations_
void
MidiDevice::SurpriseRemoval(
    PKSDEVICE Device,
    PIRP Irp
)
{
    PAGED_CODE ();

    KsAcquireDevice(Device);
    auto releaseDeviceOnExit = wil::scope_exit([Device]() { KsReleaseDevice(Device); });

    MidiDevice::Stop(Device, Irp);
}
