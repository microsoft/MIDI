// Copyright (c) Microsoft Corporation. All rights reserved.
#include "common.h"
#include "MidiPin.h"
#include "MidiFilter.h"
#include "MidiDev.h"

static const
WCHAR * g_ReferenceStrings[MAX_FILTERFACTORIES] =
{
    L"minmidi1",
    L"minmidi2",
    L"minmidi3",
    L"minmidi4",
    L"minmidi5",
    L"minmidi6",
    L"minmidi7",
    L"minmidi8",
    L"minmidi9",
    L"minmidi10",
    L"minmidi11",
    L"minmidi12",
    L"minmidi13",
    L"minmidi14",
    L"minmidi15",
    L"minmidi16",
    L"minmidi17",
    L"minmidi18",
    L"minmidi19",
    L"minmidi20"
};

static const
WCHAR * g_ReferenceStringsU[MAX_FILTERFACTORIES] =
{
    L"minmidiu1",
    L"minmidiu2",
    L"minmidiu3",
    L"minmidiu4",
    L"minmidiu5",
    L"minmidiu6",
    L"minmidiu7",
    L"minmidiu8",
    L"minmidiu9",
    L"minmidiu10",
    L"minmidiu11",
    L"minmidiu12",
    L"minmidiu13",
    L"minmidiu14",
    L"minmidiu15",
    L"minmidiu16",
    L"minmidiu17",
    L"minmidiu18",
    L"minmidiu19",
    L"minmidiu20"
};

static const
KSDEVICE_DISPATCH g_DeviceDispatch = {
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

static const
KSDEVICE_DESCRIPTOR g_DeviceDescriptor =
{
    &g_DeviceDispatch,
    0,
    NULL,
    KSDEVICE_DESCRIPTOR_VERSION
};

#define STATIC_KSPROPSETID_MinMidiControl\
            0xfe0d0e1f, 0x6c68, 0x4397, 0x8d, 0xc6, 0xa6, 0xa8, 0xdf, 0xee, 0x70, 0xa5
        DEFINE_GUIDSTRUCT("FE0D0E1F-6C68-4397-8DC6-A6A8DFEE70A5", KSPROPSETID_MinMidiControl);
#define KSPROPSETID_MinMidiControl DEFINE_GUIDNAMED(KSPROPSETID_MinMidiControl)

DECLARE_CONST_UNICODE_STRING(MinMidiControlReferenceString, L"MinMidiControl");
DECLARE_CONST_UNICODE_STRING(MinMidiControlReferenceStringSlash, L"\\MinMidiControl");

typedef enum {
    KSPROPERTY_MINMIDICONTROL_ADDPORT,
    KSPROPERTY_MINMIDICONTROL_REMOVEPORT,
    KSPROPERTY_MINMIDICONTROL_SURPRISEREMOVESIMULATION,
} KSPROPERTY_MINMIDICONTROL;

NTSTATUS
AddPortHandler
(
    PIRP irp,
    PKSPROPERTY,
    MidiDataFormats* buffer
)
{
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
    PKSDEVICE ksDevice = KsGetDeviceForDeviceObject(irpSp->DeviceObject);

    KsAcquireDevice(ksDevice);
    auto releaseDeviceOnExit = wil::scope_exit([ksDevice]() { KsReleaseDevice(ksDevice); });

    MidiDevice *This = (MidiDevice *) ksDevice->Context;
    MidiDataFormats format = (MidiDataFormats) *buffer;
    NT_RETURN_IF_NTSTATUS_FAILED(This->AddPort(ksDevice, format));

    return STATUS_SUCCESS;
}

NTSTATUS
RemovePortHandler
(
    PIRP irp,
    PKSPROPERTY,
    MidiDataFormats* buffer
)
{
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
    PKSDEVICE ksDevice = KsGetDeviceForDeviceObject(irpSp->DeviceObject);

    KsAcquireDevice(ksDevice);
    auto releaseDeviceOnExit = wil::scope_exit([ksDevice]() { KsReleaseDevice(ksDevice); });

    MidiDevice *This = (MidiDevice *) ksDevice->Context;
    MidiDataFormats format = (MidiDataFormats) *buffer;
    NT_RETURN_IF_NTSTATUS_FAILED(This->RemovePort(format));
    
    return STATUS_SUCCESS;
}

// global state used for simulating surprise removal
// for testing purposes
static bool g_bSurpriseRemoveEnabled = false;
static bool g_bSurpriseRemove = false;

NTSTATUS
SurpriseRemoveSimulationHandler
(
    PIRP,
    PKSPROPERTY,
    DWORD* buffer
)
{
    if (*buffer != 0)
    {
        g_bSurpriseRemoveEnabled = true;
        g_bSurpriseRemove = false;
    }
    else
    {
        g_bSurpriseRemoveEnabled = false;
        g_bSurpriseRemove = false;
    }

    return STATUS_SUCCESS;
}

DEFINE_KSPROPERTY_TABLE(g_MinMidiControlProperties)
{
    DEFINE_KSPROPERTY_ITEM
    (
        KSPROPERTY_MINMIDICONTROL_ADDPORT,                 // idProperty
        NULL,                                              // pfnGetHandler
        sizeof(KSPROPERTY),                                // cbMinPropertyInput
        sizeof(DWORD),                                     // cbMinDataInput
        AddPortHandler,                                    // pfnSetHandler
        0,                                                 // Values
        0,                                                 // RelationsCount
        NULL,                                              // Relations
        NULL,                                              // SupportHandler
        0                                                  // SerializedSize
    ),
    DEFINE_KSPROPERTY_ITEM
    (
        KSPROPERTY_MINMIDICONTROL_REMOVEPORT,              // idProperty
        NULL,                                              // pfnGetHandler
        sizeof(KSPROPERTY),                                // cbMinPropertyInput
        sizeof(DWORD),                                     // cbMinDataInput
        RemovePortHandler,                                 // pfnSetHandler
        0,                                                 // Values
        0,                                                 // RelationsCount
        NULL,                                              // Relations
        NULL,                                              // SupportHandler
        0                                                  // SerializedSize
    ),
    DEFINE_KSPROPERTY_ITEM
    (
        KSPROPERTY_MINMIDICONTROL_SURPRISEREMOVESIMULATION,// idProperty
        NULL,                                              // pfnGetHandler
        sizeof(KSPROPERTY),                                // cbMinPropertyInput
        sizeof(DWORD),                                     // cbMinDataInput
        SurpriseRemoveSimulationHandler,                   // pfnSetHandler
        0,                                                 // Values
        0,                                                 // RelationsCount
        NULL,                                              // Relations
        NULL,                                              // SupportHandler
        0                                                  // SerializedSize
    ),

};

DEFINE_KSPROPERTY_SET_TABLE (g_DevicePropertySets)
{
    DEFINE_KSPROPERTY_SET
    (
        &KSPROPSETID_MinMidiControl,
        SIZEOF_ARRAY(g_MinMidiControlProperties),
        g_MinMidiControlProperties,
        0,NULL
    )
};

DRIVER_UNLOAD DriverUnload;

_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
_Dispatch_type_(IRP_MJ_CREATE)
_Dispatch_type_(IRP_MJ_CLOSE)
_Function_class_(DRIVER_DISPATCH)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
MidiDeviceIrpDispatch(
    _In_ PDEVICE_OBJECT deviceObject,
    _Inout_ PIRP irp )
{
    UNREFERENCED_PARAMETER(deviceObject);

    PAGED_CODE();

    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);

    if (irpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL)
    {
        PKSPROPERTY KsProperty = (PKSPROPERTY) irpSp->Parameters.DeviceIoControl.Type3InputBuffer;

        if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_KS_PROPERTY &&
            RtlCompareMemory(&KsProperty->Set, &KSPROPSETID_MinMidiControl, sizeof(GUID)) == sizeof(GUID))
        {
            NTSTATUS status = KsPropertyHandler(irp, SIZEOF_ARRAY(g_DevicePropertySets), g_DevicePropertySets);

            irp->IoStatus.Status = status;
            IoCompleteRequest(irp, IO_NO_INCREMENT);
            return status;
        }
    }
    else if (irpSp->MajorFunction == IRP_MJ_CREATE || irpSp->MajorFunction == IRP_MJ_CLOSE)
    {
        // MinMidiControl isn't a KS filter, so we need to handle the create and close ourselves.
        if (0 == RtlCompareUnicodeString(&irpSp->FileObject->FileName, &MinMidiControlReferenceStringSlash, TRUE))
        {
            irp->IoStatus.Status = STATUS_SUCCESS;
            irp->IoStatus.Information = 0;
            IoCompleteRequest(irp, IO_NO_INCREMENT);
            return STATUS_SUCCESS;
        }
    }

    return KsDispatchIrp(deviceObject, irp);
}

_Dispatch_type_(IRP_MJ_PNP)
_Function_class_(DRIVER_DISPATCH)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
MidiDeviceIrpPNP(
    _In_ PDEVICE_OBJECT deviceObject,
    _Inout_ PIRP irp )
{
    PAGED_CODE();

    if (g_bSurpriseRemoveEnabled)
    {
        PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);

        if (IRP_MN_QUERY_REMOVE_DEVICE == irpSp->MinorFunction)
        {
            PKSDEVICE ksDevice = KsGetDeviceForDeviceObject(deviceObject);
        
            if (ksDevice != NULL)
            {
                IoInvalidateDeviceState(ksDevice->PhysicalDeviceObject);
                g_bSurpriseRemove = true;
            }
        }
        else if (IRP_MN_QUERY_PNP_DEVICE_STATE == irpSp->MinorFunction)
        {
            // if we're doing a simualted remove, return PNP device failed
            if (g_bSurpriseRemove)
            {
                irp->IoStatus.Information |= (PNP_DEVICE_FAILED);
                irp->IoStatus.Status = STATUS_SUCCESS;
                IoCompleteRequest(irp, IO_NO_INCREMENT);
                // reset back to a normal state now that we've reported
                // a PNP device failed 1 time.
                g_bSurpriseRemove = false;
                return STATUS_SUCCESS;
            }
        }
    }

    return KsDispatchIrp(deviceObject, irp);
}

PAGED_CODE_SEG
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT driverObject,
    _In_ PUNICODE_STRING registryPathName
)
{
    PAGED_CODE();
    NT_RETURN_IF_NTSTATUS_FAILED(KsInitializeDriver(driverObject, registryPathName, &g_DeviceDescriptor));

    driverObject->DriverUnload = DriverUnload;

    driverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MidiDeviceIrpDispatch;
    driverObject->MajorFunction[IRP_MJ_CREATE] = MidiDeviceIrpDispatch;
    driverObject->MajorFunction[IRP_MJ_CLOSE] = MidiDeviceIrpDispatch;

    // ***************************************************************************
    // intercept stop/remove/surprise-remove in order to simulate surprise removal
    // for testing purposes. This should never be included in a real midi driver,
    // this is for testing purposes only.
    // ***************************************************************************
    driverObject->MajorFunction[IRP_MJ_PNP] = MidiDeviceIrpPNP;

    return STATUS_SUCCESS;
}

PAGED_CODE_SEG
void
DriverUnload( 
    _In_ PDRIVER_OBJECT /* driverObject */
)
{
    PAGED_CODE();
}

_Use_decl_annotations_
MidiDevice::MidiDevice (
    PKSDEVICE device
) : m_Device(device)
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
    PKSDEVICE device
)
{
    PAGED_CODE ();

    KsAcquireDevice(device);
    auto releaseDeviceOnExit = wil::scope_exit([device]() { KsReleaseDevice(device); });

    MidiDevice *midiDevice = new (POOL_FLAG_NON_PAGED) MidiDevice(device);

    NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !midiDevice);

    device->Context = midiDevice;

    return STATUS_SUCCESS;
}

NTSTATUS
MidiDevice::AddPort
(
    PKSDEVICE device,
    MidiDataFormats formats
)
{
    // Format should be either ByteStream or UMP
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, ((formats & MidiDataFormats_ByteStream) != MidiDataFormats_ByteStream) && 
                                                      ((formats & MidiDataFormats_UMP) != MidiDataFormats_UMP));

    if (formats & MidiDataFormats_UMP)
    {
        NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, m_NumUMPFilterInstancesUsed >= MAX_FILTERFACTORIES);

        // If the filter factory for this entry hasn't been created yet, create it now.
        if (!m_UMPFilterInstance[m_NumUMPFilterInstancesUsed].FilterFactory)
        {
            m_UMPFilterInstance[m_NumUMPFilterInstancesUsed].FilterDescriptor = g_MidiFilterDescriptorUMP;
            m_UMPFilterInstance[m_NumUMPFilterInstancesUsed].ReferenceString = g_ReferenceStringsU[m_NumUMPFilterInstancesUsed];
            NT_RETURN_IF_NTSTATUS_FAILED(CreateFilterFactory(device, &m_UMPFilterInstance[m_NumUMPFilterInstancesUsed]));
        }

        // set it active
        KsFilterFactorySetDeviceClassesState(m_UMPFilterInstance[m_NumUMPFilterInstancesUsed].FilterFactory, TRUE);
        m_NumUMPFilterInstancesUsed++;
    }
    else if (formats & MidiDataFormats_ByteStream)
    {
        NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, m_NumBytestreamFilterInstancesUsed >= MAX_FILTERFACTORIES);

        // If the filter factory for this entry hasn't been created yet, create it now.
        if (!m_BytestreamFilterInstance[m_NumBytestreamFilterInstancesUsed].FilterFactory)
        {
            m_BytestreamFilterInstance[m_NumBytestreamFilterInstancesUsed].FilterDescriptor = g_MidiFilterDescriptorBytestream;
            m_BytestreamFilterInstance[m_NumBytestreamFilterInstancesUsed].ReferenceString = g_ReferenceStrings[m_NumBytestreamFilterInstancesUsed];
            NT_RETURN_IF_NTSTATUS_FAILED(CreateFilterFactory(device, &m_BytestreamFilterInstance[m_NumBytestreamFilterInstancesUsed]));
        }

        // set it active
        KsFilterFactorySetDeviceClassesState(m_BytestreamFilterInstance[m_NumBytestreamFilterInstancesUsed].FilterFactory, TRUE);
        m_NumBytestreamFilterInstancesUsed++;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
MidiDevice::RemovePort
(
    MidiDataFormats formats
)
{
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, ((formats & MidiDataFormats_ByteStream) != MidiDataFormats_ByteStream) && 
                                                      ((formats & MidiDataFormats_UMP) != MidiDataFormats_UMP));

    if (formats & MidiDataFormats_UMP)
    {
        NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, m_NumUMPFilterInstancesUsed == 0);
        m_NumUMPFilterInstancesUsed--;
        if (m_UMPFilterInstance[m_NumUMPFilterInstancesUsed].FilterFactory)
        {
            // Stop the interface but don't delete it. It'll be deleted when all of the handles are closed.
            KsFilterFactorySetDeviceClassesState(m_UMPFilterInstance[m_NumUMPFilterInstancesUsed].FilterFactory, FALSE);
        }
    }
    else if (formats & MidiDataFormats_ByteStream)
    {
        NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, m_NumBytestreamFilterInstancesUsed == 0);
        m_NumBytestreamFilterInstancesUsed--;
        if (m_BytestreamFilterInstance[m_NumBytestreamFilterInstancesUsed].FilterFactory)
        {
            // Stop the interface but don't delete it. It'll be deleted when all of the handles are closed.
            KsFilterFactorySetDeviceClassesState(m_BytestreamFilterInstance[m_NumBytestreamFilterInstancesUsed].FilterFactory, FALSE);
        }
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MidiDevice::Start(
    PKSDEVICE device,
    PIRP /* irp */,
    PCM_RESOURCE_LIST /* translatedResourceList */,
    PCM_RESOURCE_LIST /* untranslatedResourceList */
)
{
    PAGED_CODE();
    UNICODE_STRING symbolicLinkName;
    MidiDevice *This = (MidiDevice *) device->Context;

    KsAcquireDevice(device);
    auto releaseDeviceOnExit = wil::scope_exit([device]() { KsReleaseDevice(device); });

    // Activate the MinMidiControl interface, used for adding and removing ports/filter factories.
    // KSPROPSETID_MinMidiControl is also the interface class guid, for easy identification.
    NT_RETURN_IF_NTSTATUS_FAILED(IoRegisterDeviceInterface(device->PhysicalDeviceObject, &KSPROPSETID_MinMidiControl, (PUNICODE_STRING)&MinMidiControlReferenceString, &symbolicLinkName));
    auto cleanup = wil::scope_exit([&]() {
            RtlFreeUnicodeString(&symbolicLinkName);
        });

    NT_RETURN_IF_NTSTATUS_FAILED(IoSetDeviceInterfaceState(&symbolicLinkName, TRUE));
    NT_RETURN_IF_NTSTATUS_FAILED(This->AddPort(device, MidiDataFormats_UMP));
    NT_RETURN_IF_NTSTATUS_FAILED(This->AddPort(device, MidiDataFormats_ByteStream));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void 
MidiDevice::Stop(
    PKSDEVICE device,
    PIRP /* irp */
)
{
    PAGED_CODE ();

    KsAcquireDevice(device);
    auto releaseDeviceOnExit = wil::scope_exit([device]() { KsReleaseDevice(device); });

    MidiDevice * midiDevice = (MidiDevice *)device->Context;

    for (UINT i = 0; i < MAX_FILTERFACTORIES; i++)
    {
        if (midiDevice->m_UMPFilterInstance[i].FilterFactory)
        {
            // Stop the interface but don't delete it. It'll be deleted when all of the handles are closed.
            KsFilterFactorySetDeviceClassesState(midiDevice->m_UMPFilterInstance[i].FilterFactory, FALSE);
        }

        if (midiDevice->m_BytestreamFilterInstance[i].FilterFactory)
        {
            // Stop the interface but don't delete it. It'll be deleted when all of the handles are closed.
            KsFilterFactorySetDeviceClassesState(midiDevice->m_BytestreamFilterInstance[i].FilterFactory, FALSE);
        }
    }
}

_Use_decl_annotations_
void
MidiDevice::Remove(
    PKSDEVICE device,
    PIRP irp
)
{
    PAGED_CODE ();

    KsAcquireDevice(device);
    auto releaseDeviceOnExit = wil::scope_exit([device]() { KsReleaseDevice(device); });

    MidiDevice::Stop(device, irp);

    MidiDevice * midiDevice = (MidiDevice *)device->Context;
    if (midiDevice)
    {
        for (UINT i = 0; i < MAX_FILTERFACTORIES; i++)
        {
            if (midiDevice->m_UMPFilterInstance[i].FilterFactory)
            {
                PKSFILTER filter = KsFilterFactoryGetFirstChildFilter(midiDevice->m_UMPFilterInstance[i].FilterFactory);
                while (filter)
                {
                    PKSFILTER nextFilter = KsFilterGetNextSiblingFilter(filter);

                    // Get the MidiFilter context and clean it up
                    MidiFilter* midiFilter = (MidiFilter*)filter->Context;
                    if (midiFilter)
                    {
                        midiFilter->Close(filter, irp);
                    }

                    filter = nextFilter;
                }
            }

            if (midiDevice->m_BytestreamFilterInstance[i].FilterFactory)
            {
                PKSFILTER filter = KsFilterFactoryGetFirstChildFilter(midiDevice->m_BytestreamFilterInstance[i].FilterFactory);
                while (filter)
                {
                    PKSFILTER nextFilter = KsFilterGetNextSiblingFilter(filter);
                    
                    // Get the MidiFilter context and clean it up
                    MidiFilter* midiFilter = (MidiFilter*)filter->Context;
                    if (midiFilter)
                    {
                        midiFilter->Close(filter, irp);
                    }

                    filter = nextFilter;
                }
            }
        }

        delete midiDevice;
        device->Context = nullptr;
    }
}

_Use_decl_annotations_
void
MidiDevice::SurpriseRemoval(
    PKSDEVICE device,
    PIRP irp
)
{
    PAGED_CODE ();
    MidiDevice::Remove(device, irp);
}
