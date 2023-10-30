/************************************************************************************
Copyright 2023 Association of Musical Electronics Industry
Copyright 2023 Microsoft
Driver source code developed by AmeNote. Some components Copyright 2023 AmeNote Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
************************************************************************************/
/*++

Module Name:

    device.cpp - Device handling events for USBMIDI2 driver.

Abstract:

   This file contains the device entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "Pch.h"
#include "ump.h"
//#include "midi_timestamp.h"

#include "Trace.h"
#include "Device.tmh"

extern StreamEngine* g_MidiInStreamEngine;

UNICODE_STRING g_RegistryPath = {0};      // This is used to store the registry settings path for the driver

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
CopyRegistrySettingsPath(
    PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

Copies the following registry path to a global variable.

\REGISTRY\MACHINE\SYSTEM\ControlSetxxx\Services\<driver>\Parameters

Arguments:

RegistryPath - Registry path passed to DriverEntry

Returns:

NTSTATUS - SUCCESS if able to configure the framework

--*/

{
    PAGED_CODE();

    // Initializing the unicode string, so that if it is not allocated it will not be deallocated too.
    RtlInitUnicodeString(&g_RegistryPath, nullptr);

    g_RegistryPath.MaximumLength = RegistryPath->Length + sizeof(WCHAR);

    g_RegistryPath.Buffer = (PWCH)ExAllocatePool2(POOL_FLAG_PAGED, g_RegistryPath.MaximumLength, DRIVER_TAG);

    if (g_RegistryPath.Buffer == nullptr)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // ExAllocatePool2 zeros memory.

    RtlAppendUnicodeToString(&g_RegistryPath, RegistryPath->Buffer);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
EvtBusDeviceAdd(
    WDFDRIVER        Driver,
    PWDFDEVICE_INIT  DeviceInit
    )
/*++
Routine Description:

    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. We create and initialize a device object to
    represent a new instance of the device. All the software resources
    should be allocated in this callback.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS                            status;
    WDF_OBJECT_ATTRIBUTES               attributes;
    WDF_DEVICE_PNP_CAPABILITIES         pnpCaps;
    ACX_DEVICEINIT_CONFIG               devInitCfg;
    ACX_DEVICE_CONFIG                   devCfg;
    WDFDEVICE                           device = nullptr;
    PDEVICE_CONTEXT                     devCtx;
    WDF_PNPPOWER_EVENT_CALLBACKS        pnpPowerCallbacks;
    WDF_OBJECT_ATTRIBUTES               memoryAttributes;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    //
    // Allow ACX to add any pre-requirement it needs on this device.
    //
    ACX_DEVICEINIT_CONFIG_INIT(&devInitCfg);
    status = AcxDeviceInitInitialize(DeviceInit, &devInitCfg);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "AcxDeviceInitiInitialize failed %!STATUS!", status);
        goto exit;
    }

    //
    // Initialize the pnpPowerCallbacks structure.  Callback events for PNP
    // and Power are specified here.  If you don't supply any callbacks,
    // the Framework will take appropriate default actions based on whether
    // DeviceInit is initialized to be an FDO, a PDO or a filter device
    // object.
    //
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDevicePrepareHardware = EvtDevicePrepareHardware;
    pnpPowerCallbacks.EvtDeviceReleaseHardware = EvtDeviceReleaseHardware;
    pnpPowerCallbacks.EvtDeviceD0Entry = EvtDeviceD0Entry;
    pnpPowerCallbacks.EvtDeviceD0Exit = EvtDeviceD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    //
    // Specify the type of context needed. 
    // Use default locking, i.e., none.
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);
    attributes.EvtCleanupCallback = EvtDeviceContextCleanup;

    //
    // Create the device.
    //
    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDeviceCreate failed %!STATUS!", status);
        goto exit;
    }

    //
    // Init device context.
    //
    devCtx = GetDeviceContext(device);
    ASSERT(devCtx != nullptr);

    devCtx->Midi = nullptr;
    devCtx->ExcludeD3Cold = WdfFalse;

    //
    // Initialize Queues and IO Package
    // 
    status = USBMIDI2DriverQueueInitialize(device);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "USBMIDI2DriverQueueInitialize failed %!STATUS!", status);
        goto exit;
    }

    //
    // Initialize ring buffer used with continuous reader on USB Hardware
    //
    if (NT_SUCCESS(status)) {
        //
        // Create memory for Ring buffer
        //
        WDF_OBJECT_ATTRIBUTES_INIT(&memoryAttributes);
        memoryAttributes.ParentObject = device;
        status = WdfMemoryCreate(
            &memoryAttributes,
            NonPagedPoolNx,
            USBMIDI_POOLTAG,
            USBMIDI2DRIVER_RING_BUF_SIZE * sizeof(UINT32),
            &devCtx->ReadRingBuf.RingBufMemory,
            NULL
        );
        devCtx->ReadRingBuf.ringBufSize = USBMIDI2DRIVER_RING_BUF_SIZE;
        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                "WdfMemoryCreate failed for ring buffer %!STATUS!", status);
            return status;
        }
    }

    // 
    // Allow ACX to add any post-requirement it needs on this device.
    //
    ACX_DEVICE_CONFIG_INIT(&devCfg);
    status = AcxDeviceInitialize(device, &devCfg);
    if (!NT_SUCCESS(status)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "AcxDeviceInitialize failed %!STATUS!", status);
        goto exit;
    }

    //
    // Tell the framework to set the SurpriseRemovalOK in the DeviceCaps so
    // that you don't get the popup in usermode (on Win2K) when you surprise
    // remove the device.
    //
    WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);
    pnpCaps.SurpriseRemovalOK = WdfTrue;
    WdfDeviceSetPnpCapabilities(device, &pnpCaps);

    //
    // Add a MIDI device.
    //
    status = AddMidi(device);
    if (!NT_SUCCESS(status)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "AddMidi failed %!STATUS!", status);
        goto exit;
    }

    status = STATUS_SUCCESS;

exit:
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
EvtDevicePrepareHardware(
    WDFDEVICE      Device,
    WDFCMRESLIST   ResourceList,
    WDFCMRESLIST   ResourceListTranslated
    )
/*++

Routine Description:

    In this callback, the driver does whatever is necessary to make the
    hardware ready to use.  In the case of a USB device, this involves reading
    and selecting descriptors.

Arguments:

    Device - handle to a device

Return Value:

    NT status value

--*/
{
    NTSTATUS                        status;
    PDEVICE_CONTEXT                 devCtx;
    WDF_USB_DEVICE_CREATE_CONFIG    createParams;

    UNREFERENCED_PARAMETER(ResourceList);
    UNREFERENCED_PARAMETER(ResourceListTranslated);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    devCtx = GetDeviceContext(Device);
    ASSERT(devCtx != nullptr);

    //
    // Create a USB device handle so that we can communicate with the
    // underlying USB stack. The WDFUSBDEVICE handle is used to query,
    // configure, and manage all aspects of the USB device.
    // These aspects include device properties, bus properties,
    // and I/O creation and synchronization. We only create the device the first time
    // PrepareHardware is called. If the device is restarted by pnp manager
    // for resource rebalance, we will use the same device handle but then select
    // the interfaces again because the USB stack could reconfigure the device on
    // restart.
    //
    if (devCtx->UsbDevice == NULL) {

        //
        // Specifying a client contract version of 602 enables us to query for
        // and use the new capabilities of the USB driver stack for Windows 8.
        // It also implies that we conform to rules mentioned in MSDN
        // documentation for WdfUsbTargetDeviceCreateWithParameters.
        //
        WDF_USB_DEVICE_CREATE_CONFIG_INIT(&createParams,
            USBD_CLIENT_CONTRACT_VERSION_602
        );

        status = WdfUsbTargetDeviceCreateWithParameters(Device,
            &createParams,
            WDF_NO_OBJECT_ATTRIBUTES,
            &devCtx->UsbDevice
        );

        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                "WdfUsbTargetDeviceCreateWithParameters failed %!STATUS!", status);
            return status;
        }
    }

    //
    // Use helper routine to parse descriptors and select interface
    //
    status = USBMIDI2DriverSelectInterface(Device);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "USBMIDI2DriverSelectInterface failed %!STATUS!", status);
        return status;
    }

    //
    // Use helper routine to Enumerate USB pipes for use
    //
    status = USBMIDI2DriverEnumeratePipes(Device);

    //
    // Set power policy data.
    //
    status = SetPowerPolicy(Device);
    if (!NT_SUCCESS(status)) 
    {
        ASSERT(FALSE);
        goto exit;
    }

    //
    // Add static circuit to device's list.
    //
    ASSERT(devCtx->Midi);
    status = AcxDeviceAddCircuit(Device, devCtx->Midi);
    if (!NT_SUCCESS(status)) 
    {
        ASSERT(FALSE);
        goto exit;
    }

    status = STATUS_SUCCESS;

exit:
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
EvtDeviceReleaseHardware(
    WDFDEVICE      Device,
    WDFCMRESLIST   ResourceListTranslated
    )
/*++

Routine Description:

    In this callback, the driver releases the h/w resources allocated in the 
    prepare h/w callback.

Arguments:

    Device - handle to a device

Return Value:

    NT status value

--*/
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(ResourceListTranslated);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    NTSTATUS status;
    PDEVICE_CONTEXT devCtx = GetDeviceContext(Device);

    // Detach the circuit before removal
    status = AcxDeviceDetachCircuit(Device, devCtx->Midi);
    if (!NT_SUCCESS(status))
    {
        ASSERT(FALSE);
        goto exit;
    }

#if 0 // As detached, do not remove
    // Remove the circuit
    status = AcxDeviceRemoveCircuit(Device, devCtx->Midi);
    if (!NT_SUCCESS(status))
    {
        ASSERT(FALSE);
        goto exit;
    }
    devCtx->Midi = nullptr;
#endif

exit:
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS 
EvtDeviceD0Entry(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE PreviousState
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PDEVICE_CONTEXT devCtx;

    UNREFERENCED_PARAMETER(PreviousState);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    devCtx = GetDeviceContext(Device);
    ASSERT(devCtx != nullptr);

    if (devCtx->MidiInPipe)
    {
        status = WdfIoTargetStart(WdfUsbTargetPipeGetIoTarget(devCtx->MidiInPipe));
        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                "%!FUNC! Could not start interrupt pipe failed %!STATUS!", status);
        }
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
            "%!FUNC! Could not start interrupt pipe as no MidiInPipe");
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS 
EvtDeviceD0Exit(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE TargetState
    )
{
    NTSTATUS        status = STATUS_SUCCESS;
    POWER_ACTION    powerAction;
    PDEVICE_CONTEXT         devCtx;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    devCtx = GetDeviceContext(Device);
    ASSERT(devCtx != nullptr);

    powerAction = WdfDeviceGetSystemPowerAction(Device);

    // 
    // Update the power policy D3-cold info for Connected Standby.
    //
    if (TargetState == WdfPowerDeviceD3 && powerAction == PowerActionNone)
    {
        WDF_TRI_STATE           excludeD3Cold = WdfTrue;
        ACX_DX_EXIT_LATENCY     latency;
        
        //
        // Get the current exit latency.
        //
        latency = AcxDeviceGetCurrentDxExitLatency(Device, 
                                                   WdfDeviceGetSystemPowerAction(Device), 
                                                   TargetState);

        if (latency == AcxDxExitLatencyResponsive)
        {
            excludeD3Cold = WdfFalse;
        }

        if (devCtx->ExcludeD3Cold != excludeD3Cold)
        {
            devCtx->ExcludeD3Cold = excludeD3Cold;
            
            status = SetPowerPolicy(Device);
            if (!NT_SUCCESS(status))
            {
                ASSERT(FALSE);
                status = STATUS_SUCCESS;
            }
        }
    }

    if (devCtx->MidiInPipe)
    {
        WdfIoTargetStop(
            WdfUsbTargetPipeGetIoTarget(devCtx->MidiInPipe),
            WdfIoTargetCancelSentIo
        );
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
SetPowerPolicy(
    WDFDEVICE Device
    )
{
    NTSTATUS                status = STATUS_SUCCESS;
    PDEVICE_CONTEXT         devCtx;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    devCtx = GetDeviceContext(Device);
    ASSERT(devCtx != nullptr);

    //
    // Init the idle policy structure.
    //
    WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS idleSettings;
    WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(&idleSettings, IdleCannotWakeFromS0);
    idleSettings.IdleTimeout     = IDLE_POWER_TIMEOUT;
    idleSettings.IdleTimeoutType = SystemManagedIdleTimeoutWithHint;
    idleSettings.ExcludeD3Cold   = devCtx->ExcludeD3Cold;

    status = WdfDeviceAssignS0IdleSettings(Device, &idleSettings);
    if (!NT_SUCCESS(status))
    {
        ASSERT(FALSE);
        goto exit;
    }

exit:
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

_Use_decl_annotations_
PAGED_CODE_SEG
VOID
EvtDeviceContextCleanup(
    WDFOBJECT      WdfDevice
   )
/*++

Routine Description:

    In this callback, it cleans up device context.

Arguments:

    WdfDevice - WDF device object

Return Value:
    none

--*/
{
    WDFDEVICE               device;
    PDEVICE_CONTEXT         devCtx;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    device = (WDFDEVICE)WdfDevice;
    devCtx = GetDeviceContext(device);
    ASSERT(devCtx != nullptr);

    if (devCtx->Midi)
    {
        ; // TODO: CircuitCleanup(devCtx->Midi);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
USBMIDI2DriverSelectInterface(
    WDFDEVICE    Device
)
/*++

Routine Description:

    Helper routine to parse descriptors for driver and to select
    USB interface. Routine will determine if there is an Alternate
    setting for MIDI Streaming and select it, otherwise default
    to Alt Setting 0.

Arguments:

    Device - handle to a device

Return Value:

    NT status value

*/
{
    NTSTATUS                                status;
    PDEVICE_CONTEXT                         pDeviceContext;
    WDF_USB_DEVICE_INFORMATION              deviceInfo;
    ULONG                                   waitWakeEnable = 0;
    USB_DEVICE_DESCRIPTOR                   deviceDescriptor;
    PUSB_CONFIGURATION_DESCRIPTOR           pConfigurationDescriptor = NULL;
    USHORT                                  configurationDescriptorSize = 0;
    WDF_OBJECT_ATTRIBUTES                   deviceConfigAttrib;
    UINT8                                   numInterfaces;
    USB_INTERFACE_DESCRIPTOR                interfaceDescriptor;
    PWDF_USB_INTERFACE_SETTING_PAIR         pSettingPairs = NULL;
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS     configParams;
    WDF_OBJECT_ATTRIBUTES                   objectAttributes;
    PWCHAR                                  pTempBuffer = NULL;
    WDF_REQUEST_SEND_OPTIONS                reqOptions;
    USHORT                                  numChars = 0;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    pDeviceContext = GetDeviceContext(Device);

    //
    // Retrieve USBD version information, port driver capabilites and device
    // capabilites such as speed, power, etc.  
    //
    WDF_USB_DEVICE_INFORMATION_INIT(&deviceInfo);

    status = WdfUsbTargetDeviceRetrieveInformation(
        pDeviceContext->UsbDevice,
        &deviceInfo);
    if (NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "IsDeviceHighSpeed: %s\n",
            (deviceInfo.Traits & WDF_USB_DEVICE_TRAIT_AT_HIGH_SPEED) ? "TRUE" : "FALSE");
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
            "IsDeviceSelfPowered: %s\n",
            (deviceInfo.Traits & WDF_USB_DEVICE_TRAIT_SELF_POWERED) ? "TRUE" : "FALSE");

        waitWakeEnable = deviceInfo.Traits &
            WDF_USB_DEVICE_TRAIT_REMOTE_WAKE_CAPABLE;

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
            "IsDeviceRemoteWakeable: %s\n",
            waitWakeEnable ? "TRUE" : "FALSE");

        //
        // Save these for use later.
        //
        pDeviceContext->UsbDeviceTraits = deviceInfo.Traits;
    }
    else {
        pDeviceContext->UsbDeviceTraits = 0;
    }


    //
    // Get device descriptor information
    // 
    WdfUsbTargetDeviceGetDeviceDescriptor(
        pDeviceContext->UsbDevice,
        &deviceDescriptor);

    if (deviceDescriptor.bLength)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
            "Fetched device descriptor indicating:\n"
            "  bDeviceClass: 0x%x, bDeviceSubClass: 0x%x, bDeviceProtocol 0x%x\n"
            "  idVendor: 0x%04x, idProduct: 0x%04x\n"
            "  bNumConfigurations: %d\n",
            deviceDescriptor.bDeviceClass, deviceDescriptor.bDeviceSubClass, deviceDescriptor.bDeviceProtocol,
            deviceDescriptor.idVendor, deviceDescriptor.idProduct,
            deviceDescriptor.bNumConfigurations);

        /*
        * Device Descriptor information to consider to capture:
        *   idVendor
        *   idProduct
        *   Manufacture String
        *   Serial number String
        *   Number of configurations
        */
    }
    else {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting device descriptor.\n");
    }

    //
    // Get Config Descriptor
    //

    status = WdfUsbTargetDeviceRetrieveConfigDescriptor(
        pDeviceContext->UsbDevice,
        NULL,
        &configurationDescriptorSize
    );
    if (status != STATUS_BUFFER_TOO_SMALL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Configuration Descriptor Error fetching size. %!STATUS!", status);
        return(status);
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&deviceConfigAttrib);
    deviceConfigAttrib.ParentObject = pDeviceContext->UsbDevice;
    status = WdfMemoryCreate(
        &deviceConfigAttrib,
        NonPagedPoolNx,
        USBMIDI_POOLTAG,
        configurationDescriptorSize,
        &pDeviceContext->DeviceConfigDescriptorMemory,
        (PVOID*)&pConfigurationDescriptor
    );
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error allocating memory for configuration descriptor. %!STATUS!", status);
        return(status);
    }

    // Load configuration descriptor
    status = WdfUsbTargetDeviceRetrieveConfigDescriptor(
        pDeviceContext->UsbDevice,
        pConfigurationDescriptor,
        &configurationDescriptorSize
    );
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error loading configuration descriptor. %!STATUS!", status);
        return(status);
    }

    //
    // Get other useful Driver descriptor information
    // 
    // Get Serial Number string as not a current simple property to get
    if (deviceDescriptor.iSerialNumber)
    {
        WDF_REQUEST_SEND_OPTIONS_INIT(&reqOptions, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&reqOptions, WDF_REL_TIMEOUT_IN_SEC(USB_REQ_TIMEOUT_SEC));

        status = WdfUsbTargetDeviceQueryString(
            pDeviceContext->UsbDevice,
            NULL,
            &reqOptions,
            NULL,
            &numChars,
            deviceDescriptor.iSerialNumber,
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
        );
        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting Serial Number string size. %!STATUS!", status);
            return(status);
        }

        WDF_OBJECT_ATTRIBUTES_INIT(&deviceConfigAttrib);
        deviceConfigAttrib.ParentObject = pDeviceContext->UsbDevice;
        status = WdfMemoryCreate(
            &deviceConfigAttrib,
            PagedPool,
            USBMIDI_POOLTAG,
            (size_t)((numChars + 1) * sizeof(WCHAR)),
            &pDeviceContext->DeviceSNMemory,
            (PVOID*)&pTempBuffer
        );
        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error allocating memory for Serial Number. %!STATUS!", status);
            return(status);
        }
        RtlZeroMemory(pTempBuffer, (size_t)((numChars + 1) * sizeof(WCHAR)));
        status = WdfUsbTargetDeviceQueryString(
            pDeviceContext->UsbDevice,
            NULL,
            &reqOptions,
            (PUSHORT)pTempBuffer,
            &numChars,
            deviceDescriptor.iSerialNumber,
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
        );
        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error allocating memory for Serial Number. %!STATUS!", status);
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting Serial Number string." );
            return(status);
        }
    }

    // Device manufacturer (as DevicePropertyManufacturer is driver manufacturer)
    if (deviceDescriptor.iManufacturer)
    {
        WDF_REQUEST_SEND_OPTIONS_INIT(&reqOptions, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&reqOptions, WDF_REL_TIMEOUT_IN_SEC(USB_REQ_TIMEOUT_SEC));

        status = WdfUsbTargetDeviceQueryString(
            pDeviceContext->UsbDevice,
            NULL,
            &reqOptions,
            NULL,
            &numChars,
            deviceDescriptor.iManufacturer,
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
        );
        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting Manufacturer string size. %!STATUS!", status);
            return(status);
        }

        WDF_OBJECT_ATTRIBUTES_INIT(&deviceConfigAttrib);
        deviceConfigAttrib.ParentObject = pDeviceContext->UsbDevice;
        status = WdfMemoryCreate(
            &deviceConfigAttrib,
            PagedPool,
            USBMIDI_POOLTAG,
            (size_t)((numChars + 1) * sizeof(WCHAR)),
            &pDeviceContext->DeviceManfMemory,
            (PVOID*)&pTempBuffer
        );
        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error allocating memory for Manufacturer. %!STATUS!", status);
            return(status);
        }
        RtlZeroMemory(pTempBuffer, (size_t)((numChars + 1) * sizeof(WCHAR)));
        status = WdfUsbTargetDeviceQueryString(
            pDeviceContext->UsbDevice,
            NULL,
            &reqOptions,
            (PUSHORT)pTempBuffer,
            &numChars,
            deviceDescriptor.iManufacturer,
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
        );
        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting Manufacturer string %!STATUS!", status);
            return(status);
        }
    }

    // Device Product Name
    if (deviceDescriptor.iProduct)
    {
        WDF_REQUEST_SEND_OPTIONS_INIT(&reqOptions, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&reqOptions, WDF_REL_TIMEOUT_IN_SEC(USB_REQ_TIMEOUT_SEC));

        status = WdfUsbTargetDeviceQueryString(
            pDeviceContext->UsbDevice,
            NULL,
            &reqOptions,
            NULL,
            &numChars,
            deviceDescriptor.iProduct,
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
        );
        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting Product Name string size.%!STATUS!", status);
            return(status);
        }

        WDF_OBJECT_ATTRIBUTES_INIT(&deviceConfigAttrib);
        deviceConfigAttrib.ParentObject = pDeviceContext->UsbDevice;
        status = WdfMemoryCreate(
            &deviceConfigAttrib,
            PagedPool,
            USBMIDI_POOLTAG,
            (size_t)((numChars + 1) * sizeof(WCHAR)),
            &pDeviceContext->DeviceProductNameMemory,
            (PVOID*)&pTempBuffer
        );
        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error allocating memory for Product Name. %!STATUS!", status);
            return(status);
        }
        RtlZeroMemory(pTempBuffer, (size_t)((numChars + 1) * sizeof(WCHAR)));
        status = WdfUsbTargetDeviceQueryString(
            pDeviceContext->UsbDevice,
            NULL,
            &reqOptions,
            (PUSHORT)pTempBuffer,
            &numChars,
            deviceDescriptor.iProduct,
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
        );
        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting Product Name string. %!STATUS!", status);
            return(status);
        }
    }

    // Device name

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = Device;
    status = WdfDeviceAllocAndQueryProperty(Device,
        DevicePropertyFriendlyName,
        NonPagedPoolNx,
        &objectAttributes,
        &pDeviceContext->DeviceNameMemory);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Could not obtain Friendly Name String. Status: 0x%x", status);
    }
    else
    {
        // As we could not get FriendlyName means we should assign a device name for unique descriptor
        //if (pDeviceContext->DeviceProductNameMemory)
        //{

        //}
    }

    //
    // Select target interfaces
    //
    numInterfaces = WdfUsbTargetDeviceGetNumInterfaces(pDeviceContext->UsbDevice);
    // Setup store for setting pairs for use later
    if (numInterfaces)
    {
        // We only need to hold one interface
        pSettingPairs = (PWDF_USB_INTERFACE_SETTING_PAIR)ExAllocatePool2(
            POOL_FLAG_PAGED,
            sizeof(WDF_USB_INTERFACE_SETTING_PAIR) * numInterfaces,
            USBMIDI_POOLTAG
        );
    }
    // Confirm memory declared properly
    if (!pSettingPairs)
    {
        status = STATUS_SEVERITY_ERROR;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "Insufficient Resources\n");
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    // NOTE: Although the specification states that MIDI interface needs
    // an accompany Audio Control interface the Audio Control is not
    // used and many manufacturers do not include it or if combined
    // with audio interface, only have control with audio interface.
    // Therefore we will skip audio control interface.

    // Search for MIDI Interface
    pDeviceContext->UsbMIDIStreamingInterface = NULL;
    for (UINT8 interfaceCount = 0; interfaceCount < numInterfaces; interfaceCount++)
    {
        // Get the Interface
        WDFUSBINTERFACE usbInterface = WdfUsbTargetDeviceGetInterface(
            pDeviceContext->UsbDevice,
            interfaceCount
        );

        // See if there is an alternate interface
        WdfUsbInterfaceGetDescriptor(
            usbInterface,
            1,  // alternate interface
            &interfaceDescriptor
        );
        if (interfaceDescriptor.bLength)
        {
            if (interfaceDescriptor.bInterfaceClass == 1 /*AUDIO*/
                && interfaceDescriptor.bInterfaceSubClass == 3 /*MIDI STREAMING*/
                )
            {
                pDeviceContext->UsbMIDIStreamingAlt = 1;    // alternate interface
            }
            else
            {
                if (interfaceDescriptor.bInterfaceClass == 1 /*AUDIO*/
                    && interfaceDescriptor.bInterfaceSubClass == 1 /*AUDIO CONTROL*/)
                {
                    pDeviceContext->UsbControlInterface = usbInterface;
                    pSettingPairs[interfaceCount].UsbInterface = pDeviceContext->UsbControlInterface;
                    pSettingPairs[interfaceCount].SettingIndex = 1;
                }
                else
                {
                    // Unknown interface so error
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Device detected has an unknown interface.\n");
                    status = STATUS_SEVERITY_ERROR;
                    goto SelectExit;
                }
                continue; // Not MIDI so continue to next interface
            }
        }
        else
        {
            WdfUsbInterfaceGetDescriptor(
                usbInterface,
                0,  // MIDI 1.0 Interface?
                &interfaceDescriptor
            );
            if (interfaceDescriptor.bLength && interfaceDescriptor.bInterfaceClass == 1 /*AUDIO*/
                && interfaceDescriptor.bInterfaceSubClass == 3 /*MIDI STREAMING*/
                )
            {
                pDeviceContext->UsbMIDIStreamingAlt = 0;    // MIDI 1.0 Interface
            }
            else
            {
                if (interfaceDescriptor.bLength && interfaceDescriptor.bInterfaceClass == 1 /*AUDIO*/
                    && interfaceDescriptor.bInterfaceSubClass == 1 /*AUDIO CONTROL*/)
                {
                    pDeviceContext->UsbControlInterface = usbInterface;
                    pSettingPairs[interfaceCount].UsbInterface = pDeviceContext->UsbControlInterface;
                    pSettingPairs[interfaceCount].SettingIndex = 0;
                }
                else
                {
                    // Unknown interface so error
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Device detected has an unknown interface.\n");
                    status = STATUS_SEVERITY_ERROR;
                    goto SelectExit;
                }
                continue;   // Not MIDI so continue to next interface
            }
        }

        // If here, then is a MIDI interface
        pDeviceContext->UsbMIDIStreamingInterface = usbInterface;

        // Setup settings pairs for MIDI streaming interface
        pSettingPairs[interfaceCount].UsbInterface = pDeviceContext->UsbMIDIStreamingInterface;
        pSettingPairs[interfaceCount].SettingIndex = pDeviceContext->UsbMIDIStreamingAlt;
    }

    // Did we find a USB MIDI Interface?
    if (!pDeviceContext->UsbMIDIStreamingInterface)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "No USB MIDI Interface Found.\n");
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    // Prepare to select interface
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_MULTIPLE_INTERFACES(
        &configParams,
        numInterfaces,
        pSettingPairs
    );

    // Select the interfaces
    status = WdfUsbTargetDeviceSelectConfig(
        pDeviceContext->UsbDevice,
        WDF_NO_OBJECT_ATTRIBUTES,
        &configParams
    );
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "Error selecting USB Interfaces.\n");
        goto SelectExit;
    }

SelectExit:
    if (pSettingPairs)
    {
        ExFreePoolWithTag(pSettingPairs, USBMIDI_POOLTAG);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
USBMIDI2DriverEnumeratePipes(
    WDFDEVICE    Device
)
/*++

Routine Description:

    Helper routine to enumerate pipes for use by driver. Also
    configures continuous reader to manage reading USB Bulk or
    Interrupt from the connected device.

Arguments:

    Device - handle to a device

Return Value:

    NT status value

*/
{
    NTSTATUS                                status;
    PDEVICE_CONTEXT                         pDeviceContext;
    UINT8                                   numberConfiguredPipes;
    WDFUSBPIPE                              pipe;
    WDF_USB_PIPE_INFORMATION                pipeInfo;
    WDF_USB_CONTINUOUS_READER_CONFIG        readerConfig;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    pDeviceContext = GetDeviceContext(Device);

    //
    // Enumerate Pipes
    //
    numberConfiguredPipes = WdfUsbInterfaceGetNumConfiguredPipes(
        pDeviceContext->UsbMIDIStreamingInterface
    );
    if (!numberConfiguredPipes)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "No configured endpoints for MIDI streaming discovered.\n");
        return(STATUS_SEVERITY_ERROR);
    }
    // First make sure past settings for pipes cleared
    pDeviceContext->MidiInPipe = NULL;
    pDeviceContext->MidiOutPipe = NULL;
    // Now look for In and Out Pipes
    for (UINT8 count = 0; count < numberConfiguredPipes; count++)
    {
        WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);
        pipe = WdfUsbInterfaceGetConfiguredPipe(
            pDeviceContext->UsbMIDIStreamingInterface,
            count,
            &pipeInfo
        );
        if (WdfUsbTargetPipeIsInEndpoint(pipe))
        {
            pDeviceContext->MidiInPipe = pipe;
            pDeviceContext->MidiInPipeType = pipeInfo.PipeType;
            pDeviceContext->MidiInMaxSize = pipeInfo.MaximumPacketSize;
        }
        else if (WdfUsbTargetPipeIsOutEndpoint(pipe))
        {
            pDeviceContext->MidiOutPipe = pipe;
            pDeviceContext->MidiOutPipeType = pipeInfo.PipeType;
            pDeviceContext->MidiOutMaxSize = pipeInfo.MaximumPacketSize;
        }
    }
    // At least one of the endpoints needs to be found
    if (!pDeviceContext->MidiInPipe && !pDeviceContext->MidiOutPipe)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "Neither In or Out Endpoint discovered for MIDI Streaming.\n");
        return(STATUS_SEVERITY_ERROR);
    }

    //
    // Configure continuous reader
    //
#if 1
    if (pDeviceContext->MidiInPipe) // only if there is an In endpoint
    {
        WDF_USB_CONTINUOUS_READER_CONFIG_INIT(
            &readerConfig,
            USBMIDI2DriverEvtReadComplete,
            pDeviceContext,
            pDeviceContext->MidiInMaxSize);

        readerConfig.EvtUsbTargetPipeReadersFailed = USBMIDI2DriverEvtReadFailed;

        status = WdfUsbTargetPipeConfigContinuousReader(
            pDeviceContext->MidiInPipe,
            &readerConfig);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                "%!FUNC! WdfUsbTargetPipeConfigContinuousReader failed 0x%x", status);

            return status;
        }
    }
#endif
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

LONGLONG DEFAULT_CONTROL_TRANSFER_TIMEOUT = 5 * -1 * WDF_TIMEOUT_TO_SEC;

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
USBMIDI2DriverGetGTB(
    WDFDEVICE    Device
)
/*++
Routine Description:

    In this callback to fetch and parse the Group Terminal Block information
    for the case using a USB MIDI 2.0 device.

Arguments:

    Device - the device context from USB Driver

Return Value:

    NONE

--*/
{
    PDEVICE_CONTEXT devCtx = GetDeviceContext(Device);
    WDF_USB_CONTROL_SETUP_PACKET controlSetup;
    NTSTATUS status = NULL;
    midi2_desc_group_terminal_block_header_t* pGtbHeader = NULL;
    WDFMEMORY gtbMemory;
    WDF_OBJECT_ATTRIBUTES gtbMemoryAttributes;
    WDF_REQUEST_SEND_OPTIONS sendOptions;
    ULONG bytesTransferred;
    WDFREQUEST gtbRequest;
    WDF_MEMORY_DESCRIPTOR gtbMemDescriptor;
    size_t gtbMemorySize;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

#if 0
    //
    // Use helper routine to fetch and parse Group Terminal Block information
    // 
    if (devCtx->UsbMIDIStreamingAlt)
    {
        // Alternate interface - thus USB MIDI 2.0 and GTB
        status = USBMIDI2DriverGetGTB(Device);
        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                "USBMIDI2DriverGetGTB failed %!STATUS!", status);
            return status;
        }
    }
#endif

    // WDF Request
    status = WdfRequestCreate(
        WDF_NO_OBJECT_ATTRIBUTES,
        NULL,
        &gtbRequest
    );
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error allocating REQUEST memory for GTB. %!STATUS!", status);
        return(status);
    }

    // Create temporary memory for holding the GTB data from device
    WDF_OBJECT_ATTRIBUTES_INIT(&gtbMemoryAttributes);
    // Guess at an initial GTB size - if more will have to reallocate
    gtbMemorySize = sizeof(midi2_desc_group_terminal_block_header_t) + 10 * sizeof(midi2_desc_group_terminal_block_t);
    status = WdfMemoryCreate(
        &gtbMemoryAttributes,
        NonPagedPool,
        0,
        gtbMemorySize,
        &gtbMemory,
        &(void*)pGtbHeader
    );
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error allocating memory for GTB. %!STATUS!", status);
        return(status);
    }

    // Send Options
    WDF_REQUEST_SEND_OPTIONS_INIT(
        &sendOptions,
        WDF_REQUEST_SEND_OPTION_TIMEOUT
    );
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
        &sendOptions,
        DEFAULT_CONTROL_TRANSFER_TIMEOUT
    );

    USHORT interfaceNumber = (USHORT)WdfUsbInterfaceGetInterfaceNumber(devCtx->UsbControlInterface);

    // Create setup packet
    WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(
        &controlSetup,
        BmRequestDeviceToHost,
        BmRequestToInterface,
        USB_REQUEST_GET_DESCRIPTOR,
        (USHORT)0x2601,                             // Need to remove magic number
        interfaceNumber
    );

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&gtbMemDescriptor, (PVOID)pGtbHeader, gtbMemorySize);
    //WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&gtbMemDescriptor, gtbMemory, NULL);

    // maybe try above REQUEST

    status = WdfUsbTargetDeviceSendControlTransferSynchronously(
        devCtx->UsbDevice,
        gtbRequest,
        &sendOptions,
        &controlSetup,
        &gtbMemDescriptor,
        &bytesTransferred
    );
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting GTB Header. %!STATUS!", status);
        status = STATUS_SUCCESS;
        goto GetGTBExit;
    }

GetGTBExit:

    // Clean up temporary memory
    if (gtbMemory)
    {
        WdfObjectDelete(gtbMemory);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

_Use_decl_annotations_
NONPAGED_CODE_SEG
VOID USBMIDI2DriverEvtReadComplete(
    WDFUSBPIPE Pipe,
    WDFMEMORY  Buffer,
    size_t     NumBytesTransferred,
    WDFCONTEXT Context
)
/*++
Routine Description:

    In this callback for the continuous reader, the data received from the device
    and delivered to streaming interface.

Arguments:

    Pipe - the USB data pipe
    Buffer - the continuous reader provided buffer with transferred data
    NumBytesTransferred - the number of bytes transferred in Buffer
    Context - the provided Device Context

Return Value:

    NONE

--*/
{
    PDEVICE_CONTEXT     pDeviceContext = (PDEVICE_CONTEXT)Context;
    NTSTATUS            status;
    PUINT8              pReceivedBuffer;
    PUINT32             pReceivedWords;
    UMP_PACKET          umpPacket;
    UINT32              receivedIndex = 0;
    UINT32              receivedNumWords;
    struct
    {
        UMPDATAFORMAT   umpHeader;
        UINT32          umpData[4];
    } UMP_Packet_Struct;

    Pipe;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    umpPacket.wordCount = 0;

    // Confirm there is data to process
    if (NumBytesTransferred)
    {
        if (NumBytesTransferred % sizeof(UINT32))
        {
            // Expected data always 4 byte packets, if other then data transmission error
            status = STATUS_SEVERITY_WARNING;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                "Warning, data received from USB device invalid size and will be dropped.\n");
            goto ReadCompleteExit;
        }

        receivedNumWords = (UINT32)(NumBytesTransferred / sizeof(UINT32));
        pReceivedBuffer = (PUINT8)WdfMemoryGetBuffer(Buffer, NULL);
        pReceivedWords = (PUINT32)pReceivedBuffer;

        while (receivedIndex < receivedNumWords)
        {
            // Do not need to process any NULL data (NOOP in UMP)
            if (!pReceivedWords[receivedIndex])
            {
                receivedIndex++;
                continue;
            }

            // If not alternate indicating USB MIDI 1.0
            if (!pDeviceContext->UsbMIDIStreamingAlt)
            {
                // Need Cable Number
                PUINT8 pBuffer = (PUINT8)&pReceivedWords[receivedIndex];
                UINT8 cbl_num = (pBuffer[0] & 0xf0) >> 4;

                // Convert to UMP, skipping if indicated not processed
                if (!USBMIDI1ToUMP(
                    &pReceivedWords[receivedIndex++],
                    &pDeviceContext->midi1IsInSysex[cbl_num],
                    &umpPacket))
                {
                    continue;
                }

                // Convert Data into buffer to pass
                UMP_Packet_Struct.umpHeader.Position = NULL;    // For now allow service to tag time
                UMP_Packet_Struct.umpHeader.ByteCount = umpPacket.wordCount * sizeof(UINT32);
                for (UINT8 swapCount = 0; swapCount < umpPacket.wordCount; swapCount++)
                {
                    UMP_Packet_Struct.umpData[swapCount] = RtlUlongByteSwap(umpPacket.umpData.umpWords[swapCount]);
                }
            }
            else
            {
                // Put data into buffer to pass
                UMP_Packet_Struct.umpHeader.Position = NULL;    // For now allow service to tag time
                // TODO: Processing for JR Timestamps
                umpPacket.wordCount = 0;                        // use to store information

                PUINT8 pWord = (PUINT8)&pReceivedWords[receivedIndex];

                // Determine amount of data to copy based on message type
                switch (pWord[3] & 0xf0)
                {
                case UMP_MT_UTILITY:
                case UMP_MT_SYSTEM:
                case UMP_MT_MIDI1_CV:
                    umpPacket.wordCount = 1;
                    break;

                case UMP_MT_DATA_64:
                case UMP_MT_MIDI2_CV:
                    umpPacket.wordCount = 2;
                    break;

                case UMP_MT_DATA_128:
                case UMP_MT_FLEX_128:
                case UMP_MT_STREAM_128:
                    umpPacket.wordCount = 4;
                        break;

                default:
                    // Could not process packet so skip current word and try with next one
                    receivedIndex++;
                    continue;
                }
                UMP_Packet_Struct.umpHeader.ByteCount = umpPacket.wordCount * sizeof(UINT32);

                // Copy appropriate data
                RtlCopyMemory(
                    (PVOID)&UMP_Packet_Struct.umpData,
                    (PVOID)&pReceivedWords[receivedIndex],
                    UMP_Packet_Struct.umpHeader.ByteCount
                );
                receivedIndex += umpPacket.wordCount;
            }

            // Send to circuit
            if (!g_MidiInStreamEngine
                || !g_MidiInStreamEngine->FillReadStream(
                    (PUINT8)&UMP_Packet_Struct,
                    (size_t)(UMP_Packet_Struct.umpHeader.ByteCount) + sizeof(UMPDATAFORMAT)
                ))
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                    "Error submitting to read queue prep buffer.\n");
                goto ReadCompleteExit;
            }
        }
    }

ReadCompleteExit:
    // The continuous reader takes care of completion of event and clearing buffer

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return;
}


// TODO: Put into dedicated library file
_Use_decl_annotations_
NONPAGED_CODE_SEG
BOOL
USBMIDI1ToUMP(
    _In_    PUINT32         usbMidi1Pkt,
    _Inout_ bool*           pbIsInSysex,
    _Out_   PUMP_PACKET     umpPkt
)
/*++
Routine Description:

    Helper routine to handle conversion of USB MIDI 1.0 32 bit word packet
    to UMP formatted packet. The routine will only populate UMP message
    types 1: System, 2: MIDI 1.0 Channel Voice, and 3: 64 bit data (SYSEX)
    messages. It is rsponsibility of other routines to convert between
    MIDI 2.0 Channel voice if needed.

Arguments:

    usbMidi1Pkt:    The USB MIDI 1.0 packet presented as a 32 bit word
    pbIsInSysex:    Reference to boolean variable where to store if processing
                    a SYSEX message or not. Note that calling funciton is
                    responsible to maintain this state for each USB MIDI 1.0
                    data stream to process. The intial value should be false.
    umpPkt:         Reference to data location to create UMP packet into. Note
                    that for optimizations, the data beyond wordCount is not
                    cleared, therefore calling function should not process beyond
                    wordCount or zero any additional dataspace.

Return Value:

    bool:           Indicates true of umpPkt is ready, false otherwise.

--*/
{
    // Checked passed parameters
    if (!usbMidi1Pkt || !pbIsInSysex || !umpPkt)
    {
        return false;
    }

    PUCHAR pBuffer = (PUCHAR)usbMidi1Pkt;
    umpPkt->wordCount = 0;

    // Determine packet cable number from group
    UINT8 cbl_num = (pBuffer[0] & 0xf0) >> 4;

    // USB MIDI 1.0 uses a CIN as an identifier for packet, grab CIN.
    UINT8 code_index = pBuffer[0] & 0x0f;

    // Handle special case of single byte data
    if (code_index == MIDI_CIN_1BYTE_DATA && (pBuffer[1] & 0x80))
    {
        switch (pBuffer[1])
        {
        case UMP_SYSTEM_TUNE_REQ:
        case UMP_SYSTEM_TIMING_CLK:
        case UMP_SYSTEM_START:
        case UMP_SYSTEM_CONTINUE:
        case UMP_SYSTEM_STOP:
        case UMP_SYSTEM_ACTIVE_SENSE:
        case UMP_SYSTEM_RESET:
            code_index = MIDI_CIN_SYSEX_END_1BYTE;
            break;

        default:
            break;
        }
    }

    switch (code_index)
    {
    case MIDI_CIN_SYSEX_START:
        umpPkt->umpData.umpBytes[0] = UMP_MT_DATA_64 | cbl_num;   // Message Type and group

        // As this is start of SYSEX, need to set status to indicate so and copy 3 bytes of data
        umpPkt->umpData.umpBytes[1] = UMP_SYSEX7_START | 3;

        // Set that in SYSEX
        *pbIsInSysex = true;

        // Copy in rest of data
        for (int count = 1; count < 4; count++)
        {
            umpPkt->umpData.umpBytes[count + 1] = pBuffer[count];
        }
        // Pad rest of data
        umpPkt->umpData.umpBytes[5] = 0x00;
        umpPkt->umpData.umpBytes[6] = 0x00;
        umpPkt->umpData.umpBytes[7] = 0x00;

        umpPkt->wordCount = 2;
        break;

    case MIDI_CIN_SYSEX_END_1BYTE: // or single byte System Common
        // Determine if a system common
        if ((pBuffer[1] & 0x80) // most significant bit set and not sysex
            && (pBuffer[1] != MIDI_STATUS_SYSEX_END))
        {
            umpPkt->umpData.umpBytes[0] = UMP_MT_SYSTEM | cbl_num;
            umpPkt->umpData.umpBytes[1] = pBuffer[1];
            umpPkt->umpData.umpBytes[2] = 0x00;
            umpPkt->umpData.umpBytes[3] = 0x00;
            umpPkt->wordCount = 1;
            break;
        }

        umpPkt->umpData.umpBytes[0] = UMP_MT_DATA_64 | cbl_num;

        // Determine if complete based on if currently in SYSEX
        if (*pbIsInSysex)
        {
            umpPkt->umpData.umpBytes[1] = UMP_SYSEX7_END | 1;
            *pbIsInSysex = false; // we are done with SYSEX
        }
        else
        {
            umpPkt->umpData.umpBytes[1] = UMP_SYSEX7_COMPLETE | 1;
        }

        // Copy in the data, assumes the original USB MIDI 1.0 data has padded data
        for (int count = 1; count < 4; count++)
        {
            umpPkt->umpData.umpBytes[count + 1] = pBuffer[count];
        }

        umpPkt->wordCount = 2;
        break;

    case MIDI_CIN_SYSEX_END_2BYTE:
        umpPkt->umpData.umpBytes[0] = UMP_MT_DATA_64 | cbl_num;

        // Determine if complete based on if currently in SYSEX
        if (*pbIsInSysex)
        {
            umpPkt->umpData.umpBytes[1] = UMP_SYSEX7_END | 2;
            *pbIsInSysex = false; // we are done with SYSEX
        }
        else
        {
            umpPkt->umpData.umpBytes[1] = UMP_SYSEX7_COMPLETE | 2;
        }

        // Copy in the data, assumes the original USB MIDI 1.0 data has padded data
        for (int count = 1; count < 4; count++)
        {
            umpPkt->umpData.umpBytes[count + 1] = pBuffer[count];
        }
        // Pad rest of data
        umpPkt->umpData.umpBytes[6] = 0x00;
        umpPkt->umpData.umpBytes[7] = 0x00;

        umpPkt->wordCount = 2;
        break;

    case MIDI_CIN_SYSEX_END_3BYTE:
        umpPkt->umpData.umpBytes[0] = UMP_MT_DATA_64 | cbl_num;

        // Determine if complete based on if currently in SYSEX
        if (*pbIsInSysex)
        {
            umpPkt->umpData.umpBytes[1] = UMP_SYSEX7_END | 3;
            *pbIsInSysex = false; // we are done with SYSEX
        }
        else
        {
            umpPkt->umpData.umpBytes[1] = UMP_SYSEX7_COMPLETE | 3;
        }

        // Copy in the data, assumes the original USB MIDI 1.0 data has padded data
        for (int count = 1; count < 4; count++)
        {
            umpPkt->umpData.umpBytes[count + 1] = pBuffer[count];
        }
        // Pad rest of data
        umpPkt->umpData.umpBytes[6] = 0x00;
        umpPkt->umpData.umpBytes[7] = 0x00;

        umpPkt->wordCount = 2;
        break;

        // MIDI1 Channel Voice Messages
    case MIDI_CIN_NOTE_ON:
    case MIDI_CIN_NOTE_OFF:
    case MIDI_CIN_POLY_KEYPRESS:
    case MIDI_CIN_CONTROL_CHANGE:
    case MIDI_CIN_PROGRAM_CHANGE:
    case MIDI_CIN_CHANNEL_PRESSURE:
    case MIDI_CIN_PITCH_BEND_CHANGE:
        umpPkt->umpData.umpBytes[0] = UMP_MT_MIDI1_CV | cbl_num; // message type 2
        *pbIsInSysex = false; // ensure we end any current sysex packets, other layers need to handle error

        // Copy in rest of data
        for (int count = 1; count < 4; count++)
        {
            umpPkt->umpData.umpBytes[count] = pBuffer[count];
        }

        umpPkt->wordCount = 1;
        break;

    case MIDI_CIN_SYSCOM_2BYTE:
    case MIDI_CIN_SYSCOM_3BYTE:
        umpPkt->umpData.umpBytes[0] = UMP_MT_SYSTEM | cbl_num;
        for (int count = 1; count < 4; count++)
        {
            umpPkt->umpData.umpBytes[count] = pBuffer[count];
        }
        umpPkt->wordCount = 1;
        break;

    case MIDI_CIN_MISC:
    case MIDI_CIN_CABLE_EVENT:
        // These are reserved for future use and will not be translated, drop data with no processing
    default:
        // Not valid USB MIDI 1.0 transfer or NULL, skip
        return false;
    }

    return true;
}

_Use_decl_annotations_
NONPAGED_CODE_SEG
BOOLEAN USBMIDI2DriverEvtReadFailed(
    WDFUSBPIPE      Pipe,
    NTSTATUS        status,
    USBD_STATUS     UsbdStatus
)
/*++
Routine Description:

    Callback for continuous reader to indicate that it has failed read.

Arguments:

    Pipe - the USB data pipe
    status - the status of the Pipe read
    USBD_STATUS - status of usb device

Return Value:

    Boolean - true if to try to restart reader

--*/
{
    UNREFERENCED_PARAMETER(Pipe);

    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
        "%!FUNC! ReadersFailedCallback failed NTSTATUS 0x%x, UsbdStatus 0x%x\n",
        status,
        UsbdStatus);

    return TRUE;
}

_Use_decl_annotations_
NONPAGED_CODE_SEG
BOOLEAN USBMIDI2DriverFillReadQueue(
    PUINT32             pBuffer,
    size_t              bufferSize,
    PDEVICE_CONTEXT     pDeviceContext
)
/*++
Routine Description:

    Helper routine to fill ring buffer with new information to be picked up by
    the IoEvtRead routine.

Arguments:

    pBuffer - buffer to copy
    bufferSize - the number of bytes in the buffer filled.
    Context - the driver context

Return Value:

    true if successful

--*/
{
    PREAD_RING_TYPE     pRing;
    size_t              index;
    size_t              ringRemain;
    PUINT32             pRingBuffer;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    // Check parameters passed
    if (!pBuffer)
        return false;

    // For convenience
    pRing = &pDeviceContext->ReadRingBuf;
    pRingBuffer = (PUINT32)WdfMemoryGetBuffer(pRing->RingBufMemory, NULL);
    index = 0;

    // Determine if currently processing read IO
    // TODO: Need to figure out how to enable / disable this processing
    if (true && bufferSize)
    {
        // Determine size of buffer remaining
        ringRemain = (pRing->ringBufTail > pRing->ringBufHead)
            ? pRing->ringBufTail - pRing->ringBufHead
            : pRing->ringBufSize - pRing->ringBufHead + pRing->ringBufTail;

        // Is there not enough room to add to buffer
        if (bufferSize > ringRemain)
        {
            TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                "Warning, not enough room in ring buffer. Data lost.\n");
            // TODO - go through the buffer and remove old data, making sure to not split up
            // any UMP data making invalid data stream.
            // this may require spin lock protection, TBD
            // for now, just fail
            return false;
        }

        // Place data into ring buffer
        index = pRing->ringBufHead;
        for (size_t count = 0; count < bufferSize; count++)
        {
            pRingBuffer[index] = pBuffer[count];
            index = (index + 1) % pRing->ringBufSize;
        }
        pRing->ringBufHead = index;
    }
    else
    {
        // probably should clear the ring buffer if there is no
        // IO needed.
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return true;
}

_Use_decl_annotations_
NONPAGED_CODE_SEG
VOID
USBMIDI2DriverEvtIoRead(
    WDFQUEUE         Queue,
    WDFREQUEST       Request,
    size_t           Length
)
/*++

Routine Description:

    Called by the framework when it receives Read or Write requests.

Arguments:

    Queue - Default queue handle
    Request - Handle to the read/write request
    Length - Length of the data buffer associated with the request.
                 The default property of the queue is to not dispatch
                 zero length read & write requests to the driver and
                 complete is with status success. So we will never get
                 a zero length request.

Return Value:Amy


--*/
{
    NTSTATUS                    status;
    PDEVICE_CONTEXT             pDeviceContext;
    size_t                      requestSize;
    size_t                      numCopied = 0;
    PUINT32                     requestBuffer;
    WDFMEMORY                   requestMemory;
    PREAD_RING_TYPE             pRing;
    PUINT32                     pRingBuf;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    pDeviceContext = GetDeviceContext(WdfIoQueueGetDevice(Queue));
    pRing = &pDeviceContext->ReadRingBuf;
    pRingBuf = (PUINT32)WdfMemoryGetBuffer(pRing->RingBufMemory, NULL);

    // First check passed parameters
    requestSize = Length / sizeof(UINT32);
    if (!requestSize)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "%!FUNC! parameter invalid. Insufficient Length %d.\n", (int)Length);
        status = STATUS_INVALID_PARAMETER;
        goto IoEvtReadExit;
    }

    // Get request buffer to write into
    status = WdfRequestRetrieveOutputMemory(Request, &requestMemory);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WdfRequestRetrieveOutputMemory failed %!STATUS!\n", status);
        goto IoEvtReadExit;
    }
    requestBuffer = (PUINT32)WdfMemoryGetBuffer(requestMemory, NULL);

    // Transfer available data into available request buffer
    while ((pRing->ringBufTail != pRing->ringBufHead) && (numCopied < requestSize))
    {
        requestBuffer[numCopied++] = pRingBuf[pRing->ringBufTail++];
        pRing->ringBufTail %= pRing->ringBufSize;
    }

IoEvtReadExit:
    if (!NT_SUCCESS(status))
    {
        WdfRequestCompleteWithInformation(Request, status, 0);
    }
    else
    {
        WdfRequestCompleteWithInformation(Request, status, numCopied * sizeof(UINT32));
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return;
}

_Use_decl_annotations_
NONPAGED_CODE_SEG
VOID
USBMIDI2DriverIoWrite(
    WDFDEVICE  Device,
    PVOID      BufferStart,
    size_t     numBytes
)
/*++

Routine Description:

    Called by Pin to write data to USB (USB OUT).

Arguments:

    Device      The WDFDevice intstance
    BufferStart pointer to start of buffer to copy
    numBytes    The number of bytes to transfer

Return Value:Amy


--*/
{
    NTSTATUS                status;
    PDEVICE_CONTEXT         pDeviceContext = NULL;
    WDFUSBPIPE              pipe = NULL;
    PUCHAR                  pBuffer;
    PUCHAR                  pWriteBuffer = NULL;
    size_t                  writeBufferIndex = 0;
    WDFREQUEST              usbRequest = NULL;
    WDFMEMORY               writeMemory = NULL;
    PUINT32                 pWriteWords = NULL;
    WDF_OBJECT_ATTRIBUTES   writeMemoryAttributes;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    pDeviceContext = GetDeviceContext(Device);

    pipe = pDeviceContext->MidiOutPipe;
    if (!pipe)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "%!FUNC! cannot execute without valid MidiOutPipe.\n");
        status = STATUS_INVALID_PARAMETER;
        goto DriverIoWriteExit;
    }

    // 
    // General Error Checking
    // 
    if (numBytes % sizeof(UINT32)) // expected data is UINT32 array
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "%!FUNC! determined error in past data size not being UIN32 divisible.\n");
        status = STATUS_INVALID_PARAMETER;
        goto DriverIoWriteExit;
    }

    pBuffer = (PUCHAR)BufferStart;

    // Check type of device connected to
    if (!pDeviceContext->UsbMIDIStreamingAlt)
    {
        //
        // Need to convert from UMP to USB MIDI 1.0 as a USB MIDI 1.0 device connected
        //
        UINT numWords = (UINT)numBytes / sizeof(UINT32);
        UINT numProcessed = 0;
        PUINT32 words = (PUINT32)pBuffer;
        while (numProcessed < numWords)
        {
            UMP_PACKET umpPacket;
            UMP_PACKET umpWritePacket;  // used as storage to translate to USB MIDI 1.0

            umpPacket.wordCount = 0;

            // First determine the size of UMP packet based on message type
            umpPacket.umpData.umpWords[0] = RtlUlongByteSwap(words[numProcessed]);
            switch (umpPacket.umpData.umpBytes[0] & UMP_MT_MASK)
            {
            case UMP_MT_UTILITY:
            case UMP_MT_SYSTEM:
            case UMP_MT_MIDI1_CV:
            case 0x60: // undefined
            case 0x70: // undefined
                umpPacket.wordCount = 1;
                break;

            case UMP_MT_DATA_64:
            case UMP_MT_MIDI2_CV:
            case 0x80: // undefined
            case 0x90: // undefined
            case 0xa0: // undefined
                umpPacket.wordCount = 2;
                break;

            case 0xb0: // undefined
            case 0xc0: // undefined
                umpPacket.wordCount = 3;
                break;

            case UMP_MT_DATA_128:
            case UMP_MT_FLEX_128:
            case UMP_MT_STREAM_128:
            case 0xe0: // undefined
                umpPacket.wordCount = 4;
                break;

            default:
                // Unhandled or corrupt data, force to move on
                numProcessed++;
                continue;
            }

            // Confirm have enough data for full packet
            if ((numWords - numProcessed) < umpPacket.wordCount)
            {
                // If not, let system populate more
                goto DriverIoWriteExit;
            }
            // Get rest of words if needed for UMP Packet
            for (int count = 1; count < umpPacket.wordCount; count++)
            {
                umpPacket.umpData.umpWords[count] = RtlUlongByteSwap(words[numProcessed + count]);
            }

            UINT8 cbl_num = umpPacket.umpData.umpBytes[0] & UMP_GROUP_MASK; // if used, cable num is group block num

            switch (umpPacket.umpData.umpBytes[0] & UMP_MT_MASK)
            {
            case UMP_MT_SYSTEM:  // System Common messages
                umpWritePacket.wordCount = 1; // All types are single USB UMP 1.0 message
                // Now need to determine number of bytes for CIN
                switch (umpPacket.umpData.umpBytes[1])
                {
                case UMP_SYSTEM_TUNE_REQ:
                case UMP_SYSTEM_TIMING_CLK:
                case UMP_SYSTEM_START:
                case UMP_SYSTEM_CONTINUE:
                case UMP_SYSTEM_STOP:
                case UMP_SYSTEM_ACTIVE_SENSE:
                case UMP_SYSTEM_RESET:
                    umpWritePacket.umpData.umpBytes[0] = (cbl_num << 4) | MIDI_CIN_SYSEX_END_1BYTE;
                    break;

                case UMP_SYSTEM_MTC:
                case UMP_SYSTEM_SONG_SELECT:
                    umpWritePacket.umpData.umpBytes[0] = (cbl_num << 4) | MIDI_CIN_SYSCOM_2BYTE;
                    break;

                case UMP_SYSTEM_SONG_POS_PTR:
                    umpWritePacket.umpData.umpBytes[0] = (cbl_num << 4) | MIDI_CIN_SYSCOM_3BYTE;
                    break;

                default:
                    umpWritePacket.wordCount = 0;
                    break;
                }

                // Copy over actual data
                for (int count = 1; count < 4; count++)
                {
                    umpWritePacket.umpData.umpBytes[count] = umpPacket.umpData.umpBytes[count];
                }
                break;

            case UMP_MT_MIDI1_CV:
                umpWritePacket.wordCount = 1;
                umpWritePacket.umpData.umpBytes[0] = (cbl_num << 4) | ((umpPacket.umpData.umpBytes[1] & 0xf0) >> 4);
                for (int count = 1; count < 4; count++)
                {
                    umpWritePacket.umpData.umpBytes[count] = umpPacket.umpData.umpBytes[count];
                }
                break;

            case UMP_MT_DATA_64:
            {
                bool bEndSysex = false;

                // Determine if sysex will end after this message
                switch (umpPacket.umpData.umpBytes[1] & UMP_SYSEX7_STATUS_MASK)
                {
                case UMP_SYSEX7_COMPLETE:
                    pDeviceContext->midi1OutSysex[cbl_num].index = 1;
                case UMP_SYSEX7_END:
                    bEndSysex = true;
                    break;

                case UMP_SYSEX7_START:
                    pDeviceContext->midi1OutSysex[cbl_num].index = 1;
                default:
                    bEndSysex = false;
                    break;
                }

                // determine the size
                UINT8 sysexSize = umpPacket.umpData.umpBytes[1] & UMP_SYSEX7_SIZE_MASK;

                // determine the number of USB MIDI 1 packets to be created
                UINT8 numWordsNeeded = (sysexSize + pDeviceContext->midi1OutSysex[cbl_num].index - 1) / 3;
                UINT8 numBytesRemain = (sysexSize + pDeviceContext->midi1OutSysex[cbl_num].index - 1) % 3;
                if (bEndSysex && numBytesRemain)
                {
                    numWordsNeeded++; // add word for any remaining sysex data
                }
                umpWritePacket.wordCount = numWordsNeeded;

                UINT8 sysexCount = 0;
                UINT8 sysexWordCount = 0;
                while (sysexCount < sysexSize && sysexWordCount < numWordsNeeded)
                {
                    pDeviceContext->midi1OutSysex[cbl_num].buffer[pDeviceContext->midi1OutSysex[cbl_num].index++] = umpPacket.umpData.umpBytes[2 + sysexCount++];

                    // is current packet build full
                    if (pDeviceContext->midi1OutSysex[cbl_num].index == 4)
                    {
                        pDeviceContext->midi1OutSysex[cbl_num].buffer[0] = (cbl_num << 4);
                        if (bEndSysex)
                        {
                            if (sysexCount == sysexSize)
                            {
                                pDeviceContext->midi1OutSysex[cbl_num].buffer[0] |= MIDI_CIN_SYSEX_END_3BYTE;
                            }
                            else
                            {
                                pDeviceContext->midi1OutSysex[cbl_num].buffer[0] |= MIDI_CIN_SYSEX_START;
                            }
                        }
                        else
                        {
                            pDeviceContext->midi1OutSysex[cbl_num].buffer[0] |= MIDI_CIN_SYSEX_START;
                        }

                        // Fill Word into write packet and reset index
                        umpWritePacket.umpData.umpWords[sysexWordCount++] = *(PUINT32)&pDeviceContext->midi1OutSysex[cbl_num].buffer;
                        pDeviceContext->midi1OutSysex[cbl_num].index = 1;
                    }
                }

                // Determine if need to terminate current word
                if (bEndSysex && numBytesRemain)
                {
                    // Fill rest of buffer
                    while (pDeviceContext->midi1OutSysex[cbl_num].index < 4) // WRONG LOGIC HERE
                    {
                        pDeviceContext->midi1OutSysex[cbl_num].buffer[pDeviceContext->midi1OutSysex[cbl_num].index++] = 0x00;
                    }
                    switch (numBytesRemain)
                    {
                    case 1:
                        pDeviceContext->midi1OutSysex[cbl_num].buffer[0] = (cbl_num << 4) | MIDI_CIN_SYSEX_END_1BYTE;
                        break;

                    case 2:
                        pDeviceContext->midi1OutSysex[cbl_num].buffer[0] = (cbl_num << 4) | MIDI_CIN_SYSEX_END_2BYTE;
                        break;

                    default:
                        // should never get here, but use full packet in error
                        pDeviceContext->midi1OutSysex[cbl_num].buffer[0] = (cbl_num << 4) | MIDI_CIN_SYSEX_END_3BYTE;
                    }

                    // Fill Word into write packet
                    umpWritePacket.umpData.umpWords[sysexWordCount++] = *(PUINT32)&pDeviceContext->midi1OutSysex[cbl_num].buffer;
                    pDeviceContext->midi1OutSysex[cbl_num].index = 1;
                }
                break;
            }

            default:
                // Not handled so ignore
                numProcessed += umpPacket.wordCount;
                umpWritePacket.wordCount = 0;
            }

            if (umpWritePacket.wordCount)
            {
                numProcessed += umpPacket.wordCount;

                // If currently no write buffer, create one
                if (!pWriteBuffer)
                {
                    // Create Request
                    status = WdfRequestCreate(
                        NULL,       // attributes
                        WdfUsbTargetPipeGetIoTarget(pipe),
                        &usbRequest    // retuest object
                    );
                    if (!NT_SUCCESS(status))
                    {
                        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                            "%!FUNC! Error creating request for USB Write with status: %!STATUS!", status);
                        goto DriverIoWriteExit;
                    }

                    WDF_OBJECT_ATTRIBUTES_INIT(&writeMemoryAttributes);
                    writeMemoryAttributes.ParentObject = usbRequest;

                    // Create Memory Object
                    status = WdfMemoryCreate(
                        &writeMemoryAttributes,
                        NonPagedPoolNx,
                        USBMIDI_POOLTAG,
                        pDeviceContext->MidiOutMaxSize,
                        &writeMemory,
                        NULL
                    );
                    if (!NT_SUCCESS(status))
                    {
                        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                            "%!FUNC! could not create WdfMemory with status: %!STATUS!", status);
                        goto DriverIoWriteExit;
                    }
                    pWriteBuffer = (PUCHAR)WdfMemoryGetBuffer(writeMemory, NULL);
                    pWriteWords = (PUINT32)pWriteBuffer;
                }

                // Write into buffer
                for (int count = 0; count < umpWritePacket.wordCount; count++)
                {
                    pWriteWords[writeBufferIndex++] = umpWritePacket.umpData.umpWords[count];
                }

                if ((writeBufferIndex*sizeof(UINT32)) == pDeviceContext->MidiOutMaxSize)
                {
                    // Write to buffer
                    if (!USBMIDI2DriverSendToUSB(
                        usbRequest,
                        writeMemory,
                        pipe,
                        writeBufferIndex*sizeof(UINT32),
                        pDeviceContext,
                        true    // delete this request when complete
                    ))
                    {
                        goto DriverIoWriteExit;
                    }

                    // Indicate new buffer needed
                    pWriteBuffer = NULL;
                    writeBufferIndex = 0;
                }
            }
        }

        // Check if anything leftover to write to USB
        if (writeBufferIndex)
        {
            // NEED TO FILL REST OF BUFFER WITH NULL
            // 
            // Write to buffer
            if (!USBMIDI2DriverSendToUSB(
                usbRequest,
                writeMemory,
                pipe,
                writeBufferIndex*sizeof(UINT32),
                pDeviceContext,
                true    // delete this request when complete
            ))
            {
                goto DriverIoWriteExit;
            }

            // Indicate new buffer needed
            pWriteBuffer = NULL;
            writeBufferIndex = 0;
        }
    }
    else
    {
        //
        // UMP device, just pass along
        //
        size_t transferPos = 0;
        size_t thisTransferSize = 0;

        while (transferPos < numBytes)
        {
            thisTransferSize = numBytes - transferPos;

            // Enusre not bigger than max
            if (thisTransferSize > pDeviceContext->MidiOutMaxSize)
            {
                thisTransferSize = pDeviceContext->MidiOutMaxSize;
            }

            // Create request, buffer and copy data into buffer
            // Create Request
            status = WdfRequestCreate(
                NULL,       // attributes
                WdfUsbTargetPipeGetIoTarget(pipe),
                &usbRequest    // retuest object
            );
            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                    "%!FUNC! Error creating request for USB Write with status: %!STATUS!", status);
                goto DriverIoWriteExit;
            }

            WDF_OBJECT_ATTRIBUTES_INIT(&writeMemoryAttributes);
            writeMemoryAttributes.ParentObject = usbRequest;

            // Create Memory Object
            status = WdfMemoryCreate(
                &writeMemoryAttributes,
                NonPagedPoolNx,
                USBMIDI_POOLTAG,
                pDeviceContext->MidiOutMaxSize,
                &writeMemory,
                NULL
            );
            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                    "%!FUNC! could not create WdfMemory with status: %!STATUS!", status);
                goto DriverIoWriteExit;
            }

            // Copy necessary data into the buffer performing byte swap
            PUINT32 pWriteMem = (PUINT32)WdfMemoryGetBuffer(writeMemory, NULL);
            PUINT32 pReadMem = (PUINT32)&pBuffer[transferPos];
            for (int count = 0; count < (thisTransferSize / sizeof(UINT32)); count++)
            {
                pWriteMem[count] = pReadMem[count];     // USB is Little Endian format
            }

            // Transfer to USB
            if (!USBMIDI2DriverSendToUSB(
                usbRequest,
                writeMemory,
                pipe,
                thisTransferSize,
                pDeviceContext,
                true    // delete this request when complete
            ))
            {
                goto DriverIoWriteExit;
            }

            // Increment position
            transferPos += thisTransferSize;
        }
    }

DriverIoWriteExit:
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");
}

_Use_decl_annotations_
NONPAGED_CODE_SEG
BOOLEAN USBMIDI2DriverSendToUSB(
    WDFREQUEST         usbRequest,
    WDFMEMORY          reqMemory,
    WDFUSBPIPE         pipe,
    size_t             Length,
    PDEVICE_CONTEXT    pDeviceContext,
    BOOLEAN            deleteRequest
)
{
    NTSTATUS        status;
    pDeviceContext;
    WDFMEMORY_OFFSET offset;

    // define the offset
    offset.BufferLength = Length;
    offset.BufferOffset = 0;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    // Send to USB Pipe
    status = WdfUsbTargetPipeFormatRequestForWrite(pipe,
        usbRequest,
        reqMemory,
        &offset); // Offset
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WdfUsbTargetPipeFormatRequestForWrite failed %!STATUS!", status);
        goto SendToUSBExit;
    }

    WdfRequestSetCompletionRoutine(
        usbRequest,
        (deleteRequest) ? USBMIDI2DriverEvtRequestWriteCompletionRoutineDelete
        : USBMIDI2DriverEvtRequestWriteCompletionRoutine,
        pipe);

    //
    // Send the request asynchronously.
    //
    if (WdfRequestSend(usbRequest, WdfUsbTargetPipeGetIoTarget(pipe), WDF_NO_SEND_OPTIONS) == FALSE) {
        //
        // Framework couldn't send the request for some reason.
        //
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfRequestSend failed\n");
        status = WdfRequestGetStatus(usbRequest);
        goto SendToUSBExit;
    }

SendToUSBExit:
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "<-- USBUMPExpEvtIoWrite\n");

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "%!FUNC! write to USB error with status: %!STATUS!", status);
        if (!deleteRequest)
        {
            WdfRequestCompleteWithInformation(usbRequest, status, 0);
        }
        // else object will be managed by calling function

        return(false);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return true;
}

_Use_decl_annotations_
NONPAGED_CODE_SEG
VOID
USBMIDI2DriverEvtRequestWriteCompletionRoutineDelete(
    WDFREQUEST                  Request,
    WDFIOTARGET                 Target,
    PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
    WDFCONTEXT                  Context
)
/*++

Routine Description:

    This is the completion routine for writes
    If the irp completes with success, we check if we
    need to recirculate this irp for another stage of
    transfer.

Arguments:

    Context - Driver supplied context
    Device - Device handle
    Request - Request handle
    Params - request completion params

Return Value:
    None

--*/
{
    NTSTATUS    status;
    size_t      bytesWritten = 0;
    PWDF_USB_REQUEST_COMPLETION_PARAMS usbCompletionParams;

    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Context);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    status = CompletionParams->IoStatus.Status;

    //
    // For usb devices, we should look at the Usb.Completion param.
    //
    usbCompletionParams = CompletionParams->Parameters.Usb.Completion;

    bytesWritten = usbCompletionParams->Parameters.PipeWrite.Length;

    if (NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
            "Number of bytes written: %I64d\n", (INT64)bytesWritten);
    }
    else {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "Write failed: request Status 0x%x UsbdStatus 0x%x\n",
            status, usbCompletionParams->UsbdStatus);
    }

    //WdfRequestCompleteWithInformation(Request, status, bytesWritten);
    WdfObjectDelete(Request);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return;
}

_Use_decl_annotations_
NONPAGED_CODE_SEG
VOID
USBMIDI2DriverEvtRequestWriteCompletionRoutine(
    WDFREQUEST                  Request,
    WDFIOTARGET                 Target,
    PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
    WDFCONTEXT                  Context
)
/*++

Routine Description:

    This is the completion routine for writes
    If the irp completes with success, we check if we
    need to recirculate this irp for another stage of
    transfer.

Arguments:

    Context - Driver supplied context
    Device - Device handle
    Request - Request handle
    Params - request completion params

Return Value:
    None

--*/
{
    NTSTATUS    status;
    size_t      bytesWritten = 0;
    PWDF_USB_REQUEST_COMPLETION_PARAMS usbCompletionParams;

    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Context);


    status = CompletionParams->IoStatus.Status;

    //
    // For usb devices, we should look at the Usb.Completion param.
    //
    usbCompletionParams = CompletionParams->Parameters.Usb.Completion;

    bytesWritten = usbCompletionParams->Parameters.PipeWrite.Length;

    if (NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
            "Number of bytes written: %I64d\n", (INT64)bytesWritten);
    }
    else {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "Write failed: request Status 0x%x UsbdStatus 0x%x\n",
            status, usbCompletionParams->UsbdStatus);
    }

    WdfRequestCompleteWithInformation(Request, status, bytesWritten);
    //WdfObjectDelete(Request);

    return;
}
