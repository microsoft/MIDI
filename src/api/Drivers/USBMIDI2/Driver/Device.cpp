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

#include "usbdlib.h"

//#include "midi_timestamp.h"

#include "Trace.h"
#include "Device.tmh"

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
    devCtx->DeviceInGTBIDs = NULL;
    devCtx->DeviceGTBMemory = NULL;
    devCtx->DeviceOutGTBIDs = NULL;
    devCtx->DeviceWriteMemory = NULL;
    devCtx->DeviceWriteBuffer = NULL;
    devCtx->DeviceWriteBufferIndex = NULL;
  
    for (int count = 0; count < MAX_NUM_GROUPS_CABLES; count++)
    {
        devCtx->midi1IsInSysex[count] = false;
        devCtx->midi1OutSysex[count].inSysex = false;
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

exit:
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return STATUS_SUCCESS;
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
    //idleSettings.IdleTimeout     = IDLE_POWER_TIMEOUT;
    idleSettings.IdleTimeout     = 15*60*1000;
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
NTSTATUS
EvtDeviceD0Entry(
    _In_  WDFDEVICE Device,
    _In_  WDF_POWER_DEVICE_STATE PreviousState
)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(PreviousState);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
EvtDeviceD0Exit(
    _In_  WDFDEVICE Device,
    _In_  WDF_POWER_DEVICE_STATE TargetState
)
{
    NTSTATUS        status = STATUS_SUCCESS;
    POWER_ACTION    powerAction;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    PAGED_CODE();

    powerAction = WdfDeviceGetSystemPowerAction(Device);

    // 
    // Update the power policy D3-cold info for Connected Standby.
    //
    if (TargetState == WdfPowerDeviceD3 && powerAction == PowerActionNone)
    {
        PDEVICE_CONTEXT         devCtx;
        WDF_TRI_STATE           excludeD3Cold = WdfTrue;
        ACX_DX_EXIT_LATENCY     latency;

        devCtx = GetDeviceContext(Device);
        ASSERT(devCtx != nullptr);

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
            NonPagedPoolNx,
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
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting Serial Number string." );
            return(status);
        }
    }


    // VID / PID
    pDeviceContext->DeviceVID = deviceDescriptor.idVendor;
    pDeviceContext->DevicePID = deviceDescriptor.idProduct;

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
            NonPagedPoolNx,
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
            NonPagedPoolNx,
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
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Could not obtain Friendly Name String. %!STATUS!", status);
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
        pSettingPairs = (PWDF_USB_INTERFACE_SETTING_PAIR)ExAllocatePool2(
            POOL_FLAG_PAGED,
            sizeof(WDF_USB_INTERFACE_SETTING_PAIR) * numInterfaces,
            USBMIDI_POOLTAG
        );
    }
    // Confirm memory declared properly
    if (!pSettingPairs)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "Insufficient Resources %!STATUS!", status);
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    // NOTE: Although the specification states that MIDI interface needs
    // an accompany Audio Control interface the Audio Control is not
    // used and many manufacturers do not include it or if combined
    // with audio interface, only have control with audio interface.
    // Streaming MIDI interface does not use or require control
    // endpoint except by definition of being an Audio class device
    // which does by specification require a control interface.
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
                pDeviceContext->UsbMIDIStreamingAlt = 1;                    // alternate interface
                pDeviceContext->UsbMIDIInterfaceNumber = interfaceDescriptor.bInterfaceNumber;
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
                    TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "Device detected has an unknown interface, ignored.\n");
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
                pDeviceContext->UsbMIDIInterfaceNumber = interfaceDescriptor.bInterfaceNumber;
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
                    status = STATUS_INSUFFICIENT_RESOURCES;
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

        // Determine Version of the selected MIDI Interface
        PUSB_INTERFACE_DESCRIPTOR pInterfaceDescriptor = USBD_ParseConfigurationDescriptor(
            pConfigurationDescriptor,               // Descriptor Buffer
            pDeviceContext->UsbMIDIInterfaceNumber, // Interface Number
            pDeviceContext->UsbMIDIStreamingAlt     // Alternate Setting
        );
        if (!pInterfaceDescriptor)
        {
            // Unknown interface so error
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Device unable to parse for MIDI Interface.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SelectExit;
        }
        // From Interface descriptor, look for the Class-Specific MS Interface Header to get bcdMSC
        PUSB_COMMON_DESCRIPTOR pDescriptorHdr = USBD_ParseDescriptors(
            pConfigurationDescriptor,       // Descriptor Buffer
            configurationDescriptorSize,    // Descriptor Total Length
            (PVOID)pInterfaceDescriptor,    // Start search postion
            MIDI_CS_INTERFACE               // Descriptor Type
        );
        if (!pDescriptorHdr || pDescriptorHdr->bLength != 7)    // Class Specific MS Interface header length fixed
        {
            // Unknown interface so error
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Device unable to parse for MIDI MS Interface.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SelectExit;
        }
        midi_desc_header_t* pInterfaceDescHeader = (midi_desc_header_t*)pDescriptorHdr;
        pDeviceContext->UsbMIDIbcdMSC = pInterfaceDescHeader->bcdMSC;
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

    // Temporary for testing
    if (pDeviceContext->UsbMIDIbcdMSC == 0x0100)
    {
        USBMIDI2DriverCreateGTB(Device);
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
        return(STATUS_INSUFFICIENT_RESOURCES);
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
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    // Configure continuous reader
    //
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

    Function to fetch and parse the Group Terminal Block information
    for the case using a USB MIDI 2.0 device.
    If a USB MIDI 1.0 device, will call function to create Group
    Terminal Block from USB MIDI 1.0 data.

Arguments:

    Device - the device context from USB Driver

Return Value:

    NONE

--*/
{
    NTSTATUS                                    status;
    WDF_USB_CONTROL_SETUP_PACKET                controlSetupPacket;
    WDFMEMORY                                   gtbMemory = 0;
    WDF_OBJECT_ATTRIBUTES                       gtbMemoryAttributes;
    PVOID                                       gtbMemoryPtr;
    size_t                                      gtbMemorySize;
    WDF_MEMORY_DESCRIPTOR                       memoryDescriptor;
    PDEVICE_CONTEXT                             devCtx = NULL;
    ULONG                                       numBytesTransferred;
    midi2_desc_group_terminal_block_header_t    gtbHeader;
    USHORT                                      numChars = 0;
    WDF_REQUEST_SEND_OPTIONS                    reqOptions;

    if (!Device)
    {
        status = STATUS_INVALID_PARAMETER;
        goto exit;
    }
    devCtx = GetDeviceContext(Device);

    // If not USB MIDI 2.0 then try to create from USB MIDI 1.0
    if (devCtx->UsbMIDIbcdMSC != MIDI_CS_BCD_MIDI2)
    {
        status = USBMIDI2DriverCreateGTB(Device);
        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error creating GTB header for USB MIDI 1.0 device. %!STATUS!", status);
            status = STATUS_SUCCESS;
        }
        goto exit;
    }
    else
    {
        status = USBMIDI2DriverGetGTBIndexes(Device);
        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error determining GTB indexes for USB MIDI 2.0 device. %!STATUS!", status);
            status = STATUS_SUCCESS;
        }
    }

    // First get the GTB Header to determine overall size needed
    // 
    // Set memory descriptor
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memoryDescriptor, (PVOID)&gtbHeader, sizeof(gtbHeader));

    // Need interface number for setup packet
    USHORT interfaceNumber = (USHORT)WdfUsbInterfaceGetInterfaceNumber(devCtx->UsbMIDIStreamingInterface);

    // Initialize setup packet
    WDF_USB_CONTROL_SETUP_PACKET_INIT(
        &controlSetupPacket,
        BmRequestDeviceToHost,       // Direction of the request
        BmRequestToInterface,
        USB_REQUEST_GET_DESCRIPTOR,  // bRequest
        (USHORT)0x2601,              // Value
        interfaceNumber              // Index 
    );

    // NOTE: When using timeout, for some reason fails transfer. This originally worked, but after adding
    // additional parsing code below, it stopped working. Tried to multiply by 4 for 4s timeout, way too long. 
    // Still did not work. So not having timeout - this however is not a "safe" method. Needs further investigation
    // and input. This is same for next call to transfer full GTB from device.
    // 
    // Fetch the GTB header from device
    status = WdfUsbTargetDeviceSendControlTransferSynchronously(
        devCtx->UsbDevice,
        WDF_NO_HANDLE,               // Optional WDFREQUEST
        NULL, //&sendOptions,
        &controlSetupPacket,
        &memoryDescriptor,           // MemoryDescriptor
        &numBytesTransferred         // BytesTransferred 
    );
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting GTB Header. %!STATUS!", status);
        status = STATUS_SUCCESS;
        goto exit;
    }
    if (numBytesTransferred != sizeof(gtbHeader))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "GTB Header malformed.");
        status = STATUS_SUCCESS;
        goto exit;
    }

    // Get size of full GTB memory from header
    gtbMemorySize = gtbHeader.wTotalLength;

    // Create temporary memory for holding the GTB data from device
    WDF_OBJECT_ATTRIBUTES_INIT(&gtbMemoryAttributes);

    status = WdfMemoryCreate(
        &gtbMemoryAttributes,
        NonPagedPoolNx,
        0,
        gtbMemorySize,
        &gtbMemory,
        &gtbMemoryPtr
    );
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error allocating memory for GTB. %!STATUS!", status);
        goto exit;
    }

    WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&memoryDescriptor, gtbMemory, NULL);

    WDF_USB_CONTROL_SETUP_PACKET_INIT(
        &controlSetupPacket,
        BmRequestDeviceToHost,       // Direction of the request
        BmRequestToInterface,
        USB_REQUEST_GET_DESCRIPTOR,  // bRequest
        (USHORT)0x2601,              // Value
        interfaceNumber              // Index 
    );

    // Get the Group Terminal Block descriptor for this device
    status = WdfUsbTargetDeviceSendControlTransferSynchronously(
        devCtx->UsbDevice,
        WDF_NO_HANDLE,               // Optional WDFREQUEST
        NULL,//&sendOptions,
        &controlSetupPacket,
        &memoryDescriptor,           // MemoryDescriptor
        &numBytesTransferred         // BytesTransferred 
    );
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting GTB Data. %!STATUS!", status);
        status = STATUS_SUCCESS;
        goto exit;
    }

    // Determine number of GTBs in descriptor
    UINT numTerminalBlocks = (numBytesTransferred - (UINT)sizeof(midi2_desc_group_terminal_block_header_t))
        / (UINT)sizeof(midi2_desc_group_terminal_block_t);

    // Simple checks for valid data
    if (!numBytesTransferred || numTerminalBlocks > 255 ||
        ((numBytesTransferred - (UINT)sizeof(midi2_desc_group_terminal_block_header_t))
            % (UINT)sizeof(midi2_desc_group_terminal_block_t)))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error parsing GTB Data, GTB invalid size: %d", numBytesTransferred);
            status = STATUS_SUCCESS;
        goto exit;
    }

    // Map GTBs from USB
    midi2_cs_interface_desc_group_terminal_blocks* pUSBGTBs =
        (midi2_cs_interface_desc_group_terminal_blocks *)gtbMemoryPtr;

    // Get GTB ID Indexes
    size_t numInIDs = 0;
    size_t numOutIDs = 0;
    PUCHAR inIDs = NULL;
    PUCHAR outIDs = NULL;

    if (devCtx->DeviceInGTBIDs)
    {
        inIDs = (PUCHAR)WdfMemoryGetBuffer(devCtx->DeviceInGTBIDs, &numInIDs);
    }
    if (devCtx->DeviceOutGTBIDs)
    {
        outIDs = (PUCHAR)WdfMemoryGetBuffer(devCtx->DeviceOutGTBIDs, &numOutIDs);
    }

    // Determine the size of GTB Structure
    UINT grpTermBlockStructureSize = sizeof(KSMULTIPLE_ITEM);
    USHORT grpTermBlockStringSizes[255];
    bool isInIndex;
    UINT numCapturedGTBs = 0;

    for (UINT termBlockCount = 0; termBlockCount < numTerminalBlocks; termBlockCount++)
    {
        isInIndex = false;
        for (size_t count = 0; count < numInIDs; count++)
        {
            if (pUSBGTBs->aBlock[termBlockCount].bGrpTrmBlkID == inIDs[count])
            {
                isInIndex = true;
                break;
            }
        }
        for (size_t count = 0; count < numOutIDs; count++)
        {
            if (pUSBGTBs->aBlock[termBlockCount].bGrpTrmBlkID == outIDs[count])
            {
                isInIndex = true;
                break;
            }
        }

        // If not in index then cycle to next GTB
        if (!isInIndex) continue;

        numCapturedGTBs++;
        grpTermBlockStructureSize += sizeof(UMP_GROUP_TERMINAL_BLOCK_HEADER);

        // See if there is a string defined
        if (pUSBGTBs->aBlock[termBlockCount].iBlockItem)
        {
            // Find out how large this string is
            WDF_REQUEST_SEND_OPTIONS_INIT(&reqOptions, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
            WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&reqOptions, WDF_REL_TIMEOUT_IN_SEC(USB_REQ_TIMEOUT_SEC));

            status = WdfUsbTargetDeviceQueryString(
                devCtx->UsbDevice,
                NULL,
                &reqOptions,
                NULL,
                &numChars,
                pUSBGTBs->aBlock[termBlockCount].iBlockItem,
                MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
            );
            if (!NT_SUCCESS(status) || !numChars)
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting string size.%!STATUS!", status);
                status = STATUS_SUCCESS;
                // Make sure we do not try to capture this string in future processing
                pUSBGTBs->aBlock[termBlockCount].iBlockItem = 0;
                grpTermBlockStructureSize += sizeof(WCHAR);     // give space for a null character
                grpTermBlockStringSizes[termBlockCount] = sizeof(WCHAR);
                continue;
            }

            // Add space for name string including null termination
            grpTermBlockStringSizes[termBlockCount] = (numChars + 1) * (USHORT)sizeof(WCHAR);
            grpTermBlockStructureSize += grpTermBlockStringSizes[termBlockCount];
        }
        else
        {
            // Add space for name string including null termination
            grpTermBlockStringSizes[termBlockCount] = (USHORT)sizeof(WCHAR);
            grpTermBlockStructureSize += grpTermBlockStringSizes[termBlockCount];
        }
    }

    // Create memory to store GTB Parameters
    PUCHAR pGTBPropertyBuffer;
    WDF_OBJECT_ATTRIBUTES_INIT(&gtbMemoryAttributes);
    gtbMemoryAttributes.ParentObject = devCtx->UsbDevice;
    status = WdfMemoryCreate(
        &gtbMemoryAttributes,
        NonPagedPoolNx,
        USBMIDI_POOLTAG,
        (size_t)grpTermBlockStructureSize,
        &devCtx->DeviceGTBMemory,
        (PVOID*)&pGTBPropertyBuffer
    );
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error allocating memory for GTB Preoperty. %!STATUS!", status);
        return(status);
    }

    // Place KSMULTIPLE_ITEM Strucutre
    PKSMULTIPLE_ITEM pMultiHeader = (PKSMULTIPLE_ITEM)pGTBPropertyBuffer;
    pMultiHeader->Size = grpTermBlockStructureSize;
    pMultiHeader->Count = numCapturedGTBs;
    pGTBPropertyBuffer += sizeof(KSMULTIPLE_ITEM);

    for (UINT termBlockCount = 0; termBlockCount < numTerminalBlocks; termBlockCount++)
    {
        isInIndex = false;
        for (size_t count = 0; count < numInIDs; count++)
        {
            if (pUSBGTBs->aBlock[termBlockCount].bGrpTrmBlkID == inIDs[count])
            {
                isInIndex = true;
                break;
            }
        }
        for (size_t count = 0; count < numOutIDs; count++)
        {
            if (pUSBGTBs->aBlock[termBlockCount].bGrpTrmBlkID == outIDs[count])
            {
                isInIndex = true;
                break;
            }
        }

        // If not in index then cycle to next GTB
        if (!isInIndex) continue;

        PUMP_GROUP_TERMINAL_BLOCK_DEFINITION pThisGrpTrmBlk = (PUMP_GROUP_TERMINAL_BLOCK_DEFINITION)pGTBPropertyBuffer;

        // Populate data from GTB read from USB Device
        pThisGrpTrmBlk->GrpTrmBlock.Size =
            (WORD)(sizeof(UMP_GROUP_TERMINAL_BLOCK_HEADER) + grpTermBlockStringSizes[termBlockCount]);
        pThisGrpTrmBlk->GrpTrmBlock.Number = pUSBGTBs->aBlock[termBlockCount].bGrpTrmBlkID;
        pThisGrpTrmBlk->GrpTrmBlock.Direction = pUSBGTBs->aBlock[termBlockCount].bGrpTrmBlkType;
        pThisGrpTrmBlk->GrpTrmBlock.FirstGroupIndex = pUSBGTBs->aBlock[termBlockCount].nGroupTrm;
        pThisGrpTrmBlk->GrpTrmBlock.GroupCount = pUSBGTBs->aBlock[termBlockCount].nNumGroupTrm;
        pThisGrpTrmBlk->GrpTrmBlock.Protocol = pUSBGTBs->aBlock[termBlockCount].bMIDIProtocol;
        pThisGrpTrmBlk->GrpTrmBlock.MaxInputBandwidth = pUSBGTBs->aBlock[termBlockCount].wMaxInputBandwidth;
        pThisGrpTrmBlk->GrpTrmBlock.MaxOutputBandwidth = pUSBGTBs->aBlock[termBlockCount].wMaxOutputBandwidth;

        RtlZeroMemory(&pThisGrpTrmBlk->Name, grpTermBlockStringSizes[termBlockCount]);

        if (pUSBGTBs->aBlock[termBlockCount].iBlockItem)
        {
            USHORT stringSize = grpTermBlockStringSizes[termBlockCount] - sizeof(WCHAR); //Always leave NULL

            // Fetch String descriptor from device
            status = WdfUsbTargetDeviceQueryString(
                devCtx->UsbDevice,
                NULL,   // No request needed
                NULL,   // Parent already established
                (PUSHORT)&pThisGrpTrmBlk->Name,
                &stringSize,
                pUSBGTBs->aBlock[termBlockCount].iBlockItem,
                MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
            );
            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting string text.%!STATUS!", status);
                status = STATUS_SUCCESS;
                // Continue processing - string already NULL terminated blank.
            }
        }

        // Move the pointer
        pGTBPropertyBuffer += pThisGrpTrmBlk->GrpTrmBlock.Size;
    }

exit:
    if (gtbMemory)
    {
        WdfObjectDelete(gtbMemory);
    }
    return status;
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
USBMIDI2DriverCreateGTB(
    WDFDEVICE    Device
)
/*++
Routine Description:

    Function to create Group Terminal Block information
    from a USB MIDI 1.0 device.

Arguments:

    Device - the device context from USB Driver

Return Value:

    NONE

--*/
{
    PDEVICE_CONTEXT pDevCtx = NULL;
    PUSB_CONFIGURATION_DESCRIPTOR pConfigurationDescriptor = NULL;
    PUCHAR pCurrent = NULL;
    PUCHAR pEnd = NULL;
    size_t configDescSize;
    NTSTATUS status = 0;

    // Temp variables for createing GTB headers
    WDF_OBJECT_ATTRIBUTES           gtbMemoryAttributes;
    UINT8                           numMidiIn = 0;
    UINT8                           numMidiOut = 0;
    UINT8                           grpTermBlockDirection[32];
    UINT8                           grpTermBlockStringIndexes[32];
    USHORT                          grpTermBlockStringSizes[32];
    UINT8                           grpTermBlockGroupIndex[32];
    UINT8                           numGrpTermBlocks = 0;
    USHORT                          numChars = 0;
    WDF_REQUEST_SEND_OPTIONS        reqOptions;

    ASSERT(Device);
    pDevCtx = GetDeviceContext(Device);
    ASSERT(pDevCtx);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    if (pDevCtx->DeviceGTBMemory)
    {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "GTB response memory already created.");
        goto exit;
    }

    pConfigurationDescriptor = (PUSB_CONFIGURATION_DESCRIPTOR)WdfMemoryGetBuffer(pDevCtx->DeviceConfigDescriptorMemory, &configDescSize);
    // Confirm this is a USB MIDI 1.0 interface
    if (pDevCtx->UsbMIDIbcdMSC != 0x0100)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Unexpected bcdMSC version.");
        goto exit;
    }

    // Find the relevant interface
    PUSB_INTERFACE_DESCRIPTOR pInterfaceDescriptor = USBD_ParseConfigurationDescriptor(
        pConfigurationDescriptor,        // Descriptor Buffer
        pDevCtx->UsbMIDIInterfaceNumber, // Interface Number
        pDevCtx->UsbMIDIStreamingAlt     // Alternate Setting
    );
    if (!pInterfaceDescriptor)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Could not find interface descriptor for selected device.");
        goto exit;
    }
    // Set current search possition
    pCurrent = (PUCHAR)pInterfaceDescriptor + sizeof(USB_INTERFACE_DESCRIPTOR);

    // Find the end of this interface by looking for end or next interface
    PUSB_INTERFACE_DESCRIPTOR pInterfaceDescriptorNext = USBD_ParseConfigurationDescriptorEx(
        pConfigurationDescriptor,
        (PVOID)pCurrent,
        -1,         // looking for the next interface whatever it is
        -1,
        -1,
        -1,
        -1
    );
    if (!pInterfaceDescriptorNext)
    {
        // means likely last interface descriptor, thus we search to end
        pEnd = (PUCHAR)pConfigurationDescriptor + configDescSize;
    }
    else
    {
        // Next interface marks end of interface descriptor of interest
        pEnd = (PUCHAR)pInterfaceDescriptorNext;
    }

    // Now we know bounds of the descriptor, look for embedded jacks
    // NOTE: USB MIDI 1.0 implmentations did not take a consistent (or intended) approach to setting
    // IDs for Jacks in relation to the virtual cable numbers. It has been generally adopted to just
    // assume that jacks found of either IN or OUT would be enumerated as virtual cables in order they are
    // found. Therefore, the first EMBEDDED IN Jack would be assigned virtual cabled 0x00, next 0x01, etc.
    // The same for found EMBEDDED OUT Jack IDs. We will use this same approach in creating the GTB
    // representation for this device.
    
    // Setup to determine the size of GTB Structure
    UINT grpTermBlockStructureSize = sizeof(KSMULTIPLE_ITEM);

    // Walk through looking for Jack Descriptors
    PUSB_COMMON_DESCRIPTOR pNextDescriptor;
    while (nullptr != (pNextDescriptor = USBD_ParseDescriptors(
        (PVOID)pConfigurationDescriptor,
        (ULONG) configDescSize,
        (PVOID)pCurrent,
        (LONG)MIDI_CS_INTERFACE)))
    {
        // pNextDescriptor is valid, make sure in search area for this interface
        if ((PVOID)pNextDescriptor >= pEnd)
        {
            // Hit end of this interface descriptor
            break;
        }

        // Setup to look for next type descriptor
        pCurrent = (PUCHAR)pNextDescriptor + pNextDescriptor->bLength;

        // Check the subtype and process accordingly
        midi_desc_header_common_t* pCommonHdr = (midi_desc_header_common_t*)pNextDescriptor;
        midi_desc_in_jack_t* pInJack = NULL;
        midi_desc_out_jack_t* pOutJack = NULL;

        switch (pCommonHdr->bDescriptorSubType)
        {
        case MIDI_CS_INTERFACE_IN_JACK :
            if (pCommonHdr->bLength != 6)
            {
                status = STATUS_INSUFFICIENT_RESOURCES;
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "USB MIDI 1.0 In Jack Descriptor invalid format.");
                goto exit;
            }
            pInJack = (midi_desc_in_jack_t*)pNextDescriptor;
            if (pInJack->bJackType == MIDI_JACK_EMBEDDED)
            {
                // Make sure we have space to add more In GTBs
                if (numGrpTermBlocks >= 32)
                {
                    status = STATUS_INSUFFICIENT_RESOURCES;
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                        "USB MIDI 1.0 number of Embedded jacks exceed 32.");
                    goto exit;
                }
                grpTermBlockDirection[numGrpTermBlocks] = (UINT8)MIDI_CS_INTERFACE_IN_JACK;
                grpTermBlockStringIndexes[numGrpTermBlocks] = pInJack->iJack;
                grpTermBlockGroupIndex[numGrpTermBlocks] = numMidiIn++;
            }
            else
            {
                continue;
            }
            break;

        case MIDI_CS_INTERFACE_OUT_JACK :
            if (pCommonHdr->bLength < 8)
            {
                status = STATUS_INSUFFICIENT_RESOURCES;
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "USB MIDI 1.0 Out Jack Descriptor invalid format.");
                goto exit;
            }
            pOutJack = (midi_desc_out_jack_t*)pNextDescriptor;
            if (pOutJack->bJackType == MIDI_JACK_EMBEDDED)
            {
                // Make sure we have space to add more Out GTBs
                if (numGrpTermBlocks >= 32)
                {
                    status = STATUS_INSUFFICIENT_RESOURCES;
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                        "USB MIDI 1.0 number of Embedded Jacks exceeds 32.");
                    goto exit;
                }
                // Out Embedded Jack descriptor is variable size, but string index is last UINT8
                grpTermBlockDirection[numGrpTermBlocks] = (UINT8)MIDI_CS_INTERFACE_OUT_JACK;
                grpTermBlockStringIndexes[numGrpTermBlocks] = *(pCurrent - 1);
                grpTermBlockGroupIndex[numGrpTermBlocks] = numMidiOut++;
            }
            else
            {
                continue;
            }
            break;

        default :
            continue;
        }

        // Account for size if adding a GTB
        grpTermBlockStructureSize += sizeof(UMP_GROUP_TERMINAL_BLOCK_HEADER);

        if (grpTermBlockStringIndexes[numGrpTermBlocks])
        {
            // Find out how large this string is
            WDF_REQUEST_SEND_OPTIONS_INIT(&reqOptions, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
            WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&reqOptions, WDF_REL_TIMEOUT_IN_SEC(USB_REQ_TIMEOUT_SEC));

            status = WdfUsbTargetDeviceQueryString(
                pDevCtx->UsbDevice,
                NULL,
                &reqOptions,
                NULL,
                &numChars,
                grpTermBlockStringIndexes[numGrpTermBlocks],
                MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
            );
            if (!NT_SUCCESS(status) || !numChars)
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting string size.%!STATUS!", status);
                status = STATUS_SUCCESS;
                // Make sure we do not try to capture this string in future processing
                grpTermBlockStringIndexes[numGrpTermBlocks] = 0;
                grpTermBlockStringSizes[numGrpTermBlocks] = sizeof(WCHAR);
                continue;
            }

            // Add space for name string including null termination
            grpTermBlockStringSizes[numGrpTermBlocks] = (numChars + 1) * (USHORT)sizeof(WCHAR);
        }
        else
        {
            grpTermBlockStringSizes[numGrpTermBlocks] = sizeof(WCHAR);
        }
        grpTermBlockStructureSize += grpTermBlockStringSizes[numGrpTermBlocks];

        // successfully defined a group terminal block
        numGrpTermBlocks++;
    }

    // Create memory to store GTB Parameters
    PUCHAR pGTBPropertyBuffer;
    WDF_OBJECT_ATTRIBUTES_INIT(&gtbMemoryAttributes);
    gtbMemoryAttributes.ParentObject = pDevCtx->UsbDevice;
    status = WdfMemoryCreate(
        &gtbMemoryAttributes,
        NonPagedPoolNx,
        USBMIDI_POOLTAG,
        (size_t)grpTermBlockStructureSize,
        &pDevCtx->DeviceGTBMemory,
        (PVOID*)&pGTBPropertyBuffer
    );
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error allocating memory for GTB Preoperty. %!STATUS!", status);
        return(status);
    }

    // Place KSMULTIPLE_ITEM Strucutre
    PKSMULTIPLE_ITEM pMultiHeader = (PKSMULTIPLE_ITEM)pGTBPropertyBuffer;
    pMultiHeader->Size = grpTermBlockStructureSize;
    pMultiHeader->Count = numGrpTermBlocks;
    pGTBPropertyBuffer += sizeof(KSMULTIPLE_ITEM);

    for (UINT termBlockCount = 0; termBlockCount < numGrpTermBlocks; termBlockCount++)
    {
        PUMP_GROUP_TERMINAL_BLOCK_DEFINITION pThisGrpTrmBlk = (PUMP_GROUP_TERMINAL_BLOCK_DEFINITION)pGTBPropertyBuffer;

        // Populate data from GTB read from USB Device
        pThisGrpTrmBlk->GrpTrmBlock.Size =
            (WORD)(sizeof(UMP_GROUP_TERMINAL_BLOCK_HEADER) + grpTermBlockStringSizes[termBlockCount]);
        pThisGrpTrmBlk->GrpTrmBlock.Number = (BYTE) termBlockCount;
        pThisGrpTrmBlk->GrpTrmBlock.Direction =
            (grpTermBlockDirection[termBlockCount] == (UINT8)MIDI_CS_INTERFACE_IN_JACK) ?
            0x01 /*Input Only*/ : 0x02 /*Output Only*/;
        pThisGrpTrmBlk->GrpTrmBlock.FirstGroupIndex = grpTermBlockGroupIndex[termBlockCount];
        pThisGrpTrmBlk->GrpTrmBlock.GroupCount = 1;
        pThisGrpTrmBlk->GrpTrmBlock.Protocol = 0x01 /*MIDI_1_0_UP_TO_64_BITS*/;
        pThisGrpTrmBlk->GrpTrmBlock.MaxInputBandwidth = 0x0000; // Unknown or not fixed
        pThisGrpTrmBlk->GrpTrmBlock.MaxOutputBandwidth = 0x0000; // Unknown or not fixed

        RtlZeroMemory(&pThisGrpTrmBlk->Name, grpTermBlockStringSizes[termBlockCount]);

        if (grpTermBlockStringIndexes[termBlockCount])
        {
            USHORT stringSize = grpTermBlockStringSizes[termBlockCount] - sizeof(WCHAR); //Always leave NULL

            // Fetch String descriptor from device
            status = WdfUsbTargetDeviceQueryString(
                pDevCtx->UsbDevice,
                NULL,   // No request needed
                NULL,   // Parent already established
                (PUSHORT)&pThisGrpTrmBlk->Name,
                &stringSize,
                grpTermBlockStringIndexes[termBlockCount],
                MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
            );
            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting string text.%!STATUS!", status);
                status = STATUS_SUCCESS;
                // Continue processing - string already NULL terminated blank.
            }
        }

        // Move the pointer
        pGTBPropertyBuffer += pThisGrpTrmBlk->GrpTrmBlock.Size;
    }

exit:
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");
    return status;
}

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
USBMIDI2DriverGetGTBIndexes(
    _In_    WDFDEVICE      Device
)
/*++
Routine Description:

    This is a helper routine to fetch the GTB IDs indexed by Endpoints for this
    device interface.

Arguments:
    Device - the device context from USB Driver
    Indexes - pointer to array of indexes to be populated with indexes found.

Return Value:

    NONE

--*/
{
    PDEVICE_CONTEXT pDevCtx = NULL;
    PUSB_CONFIGURATION_DESCRIPTOR pConfigurationDescriptor = NULL;
    PUCHAR pCurrent = NULL;
    PUCHAR pEnd = NULL;
    size_t configDescSize;
    NTSTATUS status = 0;
    WDF_OBJECT_ATTRIBUTES deviceConfigAttrib;
    bool isInEndpoint = false;
    PUSB_ENDPOINT_DESCRIPTOR pEndpointDescriptor = NULL;

    ASSERT(Device);
    pDevCtx = GetDeviceContext(Device);
    ASSERT(pDevCtx);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    if (pDevCtx->DeviceGTBMemory)
    {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "GTB response memory already created.");
        goto exit;
    }

    pConfigurationDescriptor = (PUSB_CONFIGURATION_DESCRIPTOR)WdfMemoryGetBuffer(pDevCtx->DeviceConfigDescriptorMemory, &configDescSize);
    // Confirm this is a USB MIDI 2.0 device interface
    if (pDevCtx->UsbMIDIbcdMSC != 0x200)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Unexpected bcdMSC version.");
        goto exit;
    }
 
    // Find the relevant interface
    PUSB_INTERFACE_DESCRIPTOR pInterfaceDescriptor = USBD_ParseConfigurationDescriptor(
        pConfigurationDescriptor,        // Descriptor Buffer
        pDevCtx->UsbMIDIInterfaceNumber, // Interface Number
        pDevCtx->UsbMIDIStreamingAlt     // Alternate Setting
    );
    if (!pInterfaceDescriptor)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Could not find interface descriptor for selected device.");
        goto exit;
    }
    // Set current search possition
    pCurrent = (PUCHAR)pInterfaceDescriptor + sizeof(USB_INTERFACE_DESCRIPTOR);

    // Find the end of this interface by looking for end or next interface
    PUSB_INTERFACE_DESCRIPTOR pInterfaceDescriptorNext = USBD_ParseConfigurationDescriptorEx(
        pConfigurationDescriptor,
        (PVOID)pCurrent,
        -1,         // looking for the next interface whatever it is
        -1,
        -1,
        -1,
        -1
    );
    if (!pInterfaceDescriptorNext)
    {
        // means likely last interface descriptor, thus we search to end
        pEnd = (PUCHAR)pConfigurationDescriptor + configDescSize;
    }
    else
    {
        // Next interface marks end of interface descriptor of interest
        pEnd = (PUCHAR)pInterfaceDescriptorNext;
    }

    // Now we know the bounds of the descriptor, look for endpoints to determine GTB ID Indexes.
    // Walk through finding Class-Specific MIDI Streaming Data Endpoint Descriptors
    // Walk through looking for Jack Descriptors
    PUSB_COMMON_DESCRIPTOR pNextDescriptor;
    while (nullptr != (pNextDescriptor = USBD_ParseDescriptors(
        (PVOID)pConfigurationDescriptor,
        (ULONG)configDescSize,
        (PVOID)pCurrent,
        (LONG)USB_ENDPOINT_DESCRIPTOR_TYPE)))
    {
        // pNextDescriptor is valid, make sure in search area for this interface
        if ((PVOID)pNextDescriptor >= pEnd)
        {
            // Hit end of this interface descriptor
            break;
        }

        // Get info from Endpoint Descriptor
        pEndpointDescriptor = (PUSB_ENDPOINT_DESCRIPTOR)pNextDescriptor;
        ASSERT(pEndpointDescriptor);
        isInEndpoint = (pEndpointDescriptor->bEndpointAddress & 0x80) ? true : false;

        // Advance to next descriptor which should be Class Specific Endpoint Descriptor
        pCurrent = (PUCHAR)pNextDescriptor + pNextDescriptor->bLength;

        // Check the subtype and process accordingly
        midi_desc_header_common_t* pCommonHdr = (midi_desc_header_common_t*)pCurrent;
        
        // Is it correct descriptor
        if (pCommonHdr->bDescriptorType != MIDI_CS_ENDPOINT || pCommonHdr->bDescriptorSubType != MIDI20_CS_ENDPOINT_GENERAL)
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Expected MIDI2.0 CS Endpoint not found.");
            goto exit;
        }

        midi2_desc_streaming_data_endpoint_t* pCSEndpointHdr = (midi2_desc_streaming_data_endpoint_t*)pCommonHdr;

        // If there are defined GTB IDs and the device does not already have defined GTB IDs for endpoint
        // then create and load
        if (pCSEndpointHdr->bNumGrpTrmBlock && (isInEndpoint ? !pDevCtx->DeviceInGTBIDs : !pDevCtx->DeviceOutGTBIDs))
        {
            // Create temporary memory for holding the GTB data from device
            WDF_OBJECT_ATTRIBUTES_INIT(&deviceConfigAttrib);
            deviceConfigAttrib.ParentObject = pDevCtx->UsbDevice;
            PUCHAR pGTBIDs;
            status = WdfMemoryCreate(
                &deviceConfigAttrib,
                NonPagedPoolNx,
                USBMIDI_POOLTAG,
                (size_t)pCSEndpointHdr->bNumGrpTrmBlock,
                (isInEndpoint) ? &pDevCtx->DeviceInGTBIDs : &pDevCtx->DeviceOutGTBIDs,
                (PVOID*)&pGTBIDs
            );
            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error allocating memory for GTB IDs. %!STATUS!", status);
                goto exit;
            }
            RtlCopyMemory(pGTBIDs, (PUCHAR)&pCSEndpointHdr->bAssoGrpTrmBlkID, (size_t)pCSEndpointHdr->bNumGrpTrmBlock);
        }
    }

exit:
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
    static UMP_PACKET   umpPacket;          // Keep to fill over two USB packets
    UINT32              receivedIndex = 0;
    UINT32              receivedNumWords;
    static struct
    {
        UMPDATAFORMAT   umpHeader;
        UINT32          umpData[4];
    } UMP_Packet_Struct;
    static UINT32       wordsRemain = 0;    // To know how many words remaining to process

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

            // If indicating USB MIDI 1.0
            if (pDeviceContext->UsbMIDIbcdMSC == MIDI_CS_BCD_MIDI1)
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
            else if (pDeviceContext->UsbMIDIbcdMSC == MIDI_CS_BCD_MIDI2)
            {
                // Determine if there remains data to transfer meaning UMP broken over two USB packets
                if (wordsRemain)
                {
                    UINT32 bytesToCopy;
                    if (wordsRemain <= (receivedNumWords - receivedIndex))
                    {
                        bytesToCopy = wordsRemain * sizeof(UINT32);
                        wordsRemain = 0;
                    }
                    else
                    {
                        bytesToCopy = (receivedNumWords - receivedIndex) * sizeof(UINT32);
                        wordsRemain -= bytesToCopy / sizeof(UINT32);
                    }

                    // Copy more data from bridging UMP over USB packets
                    RtlCopyMemory(
                        (PVOID)&UMP_Packet_Struct.umpData[UMP_Packet_Struct.umpHeader.ByteCount / sizeof(UINT32)],
                        (PVOID)&pReceivedWords[receivedIndex],
                        bytesToCopy
                    );
                    UMP_Packet_Struct.umpHeader.ByteCount += bytesToCopy;
                    receivedIndex += bytesToCopy / sizeof(UINT32);
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
                    case UMP_MT_RESERVED_6:
                    case UMP_MT_RESERVED_7:
                        umpPacket.wordCount = 1;
                        break;

                    case UMP_MT_DATA_64:
                    case UMP_MT_MIDI2_CV:
                    case UMP_MT_RESERVED_8:
                    case UMP_MT_RESERVED_9:
                    case UMP_MT_RESERVED_A:
                        umpPacket.wordCount = 2;
                        break;

                    case UMP_MT_RESERVED_B:
                    case UMP_MT_RESERVED_C:
                        umpPacket.wordCount = 3;
                        break;

                    case UMP_MT_DATA_128:
                    case UMP_MT_FLEX_128:
                    case UMP_MT_STREAM_128:
                    case UMP_MT_RESERVED_E:
                        umpPacket.wordCount = 4;
                        break;

                    default:
                        // Could not process packet so skip current word and try with next one
                        receivedIndex++;
                        continue;
                    }

                    // Determine if all data available to complete UMP packet
                    if (umpPacket.wordCount <= (receivedNumWords - receivedIndex))
                    {
                        UMP_Packet_Struct.umpHeader.ByteCount = umpPacket.wordCount * sizeof(UINT32);
                        wordsRemain = 0;    // no more to fill data
                    }
                    else
                    {
                        UMP_Packet_Struct.umpHeader.ByteCount = (receivedNumWords - receivedIndex) * sizeof(UINT32);
                        wordsRemain = umpPacket.wordCount - (receivedNumWords - receivedIndex);
                    }

                    // Copy appropriate data
                    RtlCopyMemory(
                        (PVOID)&UMP_Packet_Struct.umpData,
                        (PVOID)&pReceivedWords[receivedIndex],
                        UMP_Packet_Struct.umpHeader.ByteCount
                    );
                    receivedIndex += UMP_Packet_Struct.umpHeader.ByteCount / sizeof(UINT32);
                }
            }
            else
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                    "Unknown USB MIDI BCD Type.\n");
                goto ReadCompleteExit;
            }

            // Send to circuit
            if (!wordsRemain)
            {
                if (pDeviceContext->pStreamEngine)
                {
                    if (!pDeviceContext->pStreamEngine->FillReadStream(
                        (PUINT8)&UMP_Packet_Struct,
                        (size_t)(UMP_Packet_Struct.umpHeader.ByteCount) + sizeof(UMPDATAFORMAT)
                    ))
                    {
                        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                            "Error submitting to read queue prep buffer.\n");
                        goto ReadCompleteExit;
                    }
                }
                else
                {
                    TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                        "No StreamEngine found in Device Context for filling read stream.\n");
                    goto ReadCompleteExit;
                }
            }
        }
    }

ReadCompleteExit:
    // The continuous reader takes care of completion of event and clearing buffer

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return;
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
VOID
USBMIDI2DriverIoWrite(
    WDFDEVICE  Device,
    PVOID      BufferStart,
    size_t     numBytes,
    BOOLEAN    isMoreData
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
    PUINT32                 pWriteWords = NULL;
    WDF_OBJECT_ATTRIBUTES   writeMemoryAttributes;
    bool bEnterSysex;
    bool bEndSysex;
    UINT8 numberBytes;
    UINT8 sysexStatus;
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
    if (pDeviceContext->UsbMIDIbcdMSC == MIDI_CS_BCD_MIDI1)
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
            case UMP_MT_RESERVED_6:
            case UMP_MT_RESERVED_7:
                umpPacket.wordCount = 1;
                break;

            case UMP_MT_DATA_64:
            case UMP_MT_MIDI2_CV:
            case UMP_MT_RESERVED_8:
            case UMP_MT_RESERVED_9:
            case UMP_MT_RESERVED_A:
                umpPacket.wordCount = 2;
                break;

            case UMP_MT_RESERVED_B:
            case UMP_MT_RESERVED_C:
                umpPacket.wordCount = 3;
                break;

            case UMP_MT_DATA_128:
            case UMP_MT_FLEX_128:
            case UMP_MT_STREAM_128:
            case UMP_MT_RESERVED_E:
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
                case UMP_SYSTEM_UNDEFINED_F4:
                case UMP_SYSTEM_UNDEFINED_F5:
                case UMP_SYSTEM_UNDEFINED_F9:
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
                bEnterSysex = false;
                bEndSysex = false;

                umpWritePacket.wordCount = 0;

                // Determine if sysex will end after this message
                switch (umpPacket.umpData.umpBytes[1] & UMP_SYSEX7_STATUS_MASK)
                {
                case UMP_SYSEX7_COMPLETE:
                    bEnterSysex = true;
                case UMP_SYSEX7_END:
                    bEndSysex = true;
                    break;

                case UMP_SYSEX7_START:
                    bEnterSysex = true;
                default:
                    bEndSysex = false;
                    break;
                }

                if (bEnterSysex)
                {
                    // Determine if believed already in Sysex and if so, reset converter
                    if (pDeviceContext->midi1OutSysex[cbl_num].inSysex)
                    {
                        pDeviceContext->midi1OutSysex[cbl_num].inSysex = false;
                    }
                }

                if (bEnterSysex && !pDeviceContext->midi1OutSysex[cbl_num].inSysex)
                {
                    pDeviceContext->midi1OutSysex[cbl_num].usbMIDI1Head = 0;
                    pDeviceContext->midi1OutSysex[cbl_num].usbMIDI1Tail = 0;
                    pDeviceContext->midi1OutSysex[cbl_num].inSysex = true;
                }

                UINT8 byteStream[UMPTOBS_BUFFER];
                sysexStatus = (umpPacket.umpData.umpBytes[1] >> 4);
                numberBytes = 0;

                if (sysexStatus <= 1 && numberBytes < UMPTOBS_BUFFER)
                {
                    byteStream[numberBytes++] = SYSEX_START;
                }
                for (UINT8 count = 0; count < (umpPacket.umpData.umpBytes[1] & 0xf); count++)
                {
                    if (numberBytes < UMPTOBS_BUFFER)
                    {
                        byteStream[numberBytes++] = umpPacket.umpData.umpBytes[2 + count];
                    }
                }
                if ((sysexStatus == 0 || sysexStatus == 3) && numberBytes < UMPTOBS_BUFFER)
                {
                    byteStream[numberBytes++] = SYSEX_STOP;
                }

                // Move into sysex circular buffer queue
                for (UINT8 count = 0; count < numberBytes; count++)
                {
                    pDeviceContext->midi1OutSysex[cbl_num].sysexBS[pDeviceContext->midi1OutSysex[cbl_num].usbMIDI1Head++]
                        = byteStream[count];
                    pDeviceContext->midi1OutSysex[cbl_num].usbMIDI1Head %= SYSEX_BS_RB_SIZE;
                }

                // How many bytes available in BS
                numberBytes = (pDeviceContext->midi1OutSysex[cbl_num].usbMIDI1Head > pDeviceContext->midi1OutSysex[cbl_num].usbMIDI1Tail)
                    ? pDeviceContext->midi1OutSysex[cbl_num].usbMIDI1Head - pDeviceContext->midi1OutSysex[cbl_num].usbMIDI1Tail
                    : (SYSEX_BS_RB_SIZE - pDeviceContext->midi1OutSysex[cbl_num].usbMIDI1Head) + pDeviceContext->midi1OutSysex[cbl_num].usbMIDI1Tail;

                while (numberBytes && umpWritePacket.wordCount < 4)
                {
                    umpWritePacket.umpData.umpWords[umpWritePacket.wordCount] = 0;

                    if (numberBytes > 2)
                    {
                        PUINT8 pumpBytes = (PUINT8) & umpWritePacket.umpData.umpWords[umpWritePacket.wordCount];
                        for (UINT8 count = 0; count < 3; count++)
                        {
                            pumpBytes[count + 1] =
                                pDeviceContext->midi1OutSysex[cbl_num].sysexBS[pDeviceContext->midi1OutSysex[cbl_num].usbMIDI1Tail++];
                            pDeviceContext->midi1OutSysex[cbl_num].usbMIDI1Tail %= SYSEX_BS_RB_SIZE;
                            numberBytes--;
                        }
                        // Mark cable number and CIN for start / continue SYSEX in USB MIDI 1.0 format
                        if (bEndSysex && !numberBytes)
                        {
                            pumpBytes[0] = (UINT8)(cbl_num << 4) | MIDI_CIN_SYSEX_END_3BYTE;
                        }
                        else
                        {
                            pumpBytes[0] = (UINT8)(cbl_num << 4) | MIDI_CIN_SYSEX_START;
                        }
                    }
                    else
                    {
                        // If less than two and we have a word to populate, check if the end of sysex
                        if (bEndSysex)
                        {
                            // Process bytes
                            PUINT8 pumpBytes = (PUINT8)&umpWritePacket.umpData.umpWords[umpWritePacket.wordCount];
                            UINT8 count;
                            for (count = 0; numberBytes; count++)
                            {
                                pumpBytes[count + 1] =
                                    pDeviceContext->midi1OutSysex[cbl_num].sysexBS[pDeviceContext->midi1OutSysex[cbl_num].usbMIDI1Tail++];
                                pDeviceContext->midi1OutSysex[cbl_num].usbMIDI1Tail %= SYSEX_BS_RB_SIZE;
                                numberBytes--;
                            }
                            // Mark cable number and CIN for start / continue SYSEX in USB MIDI 1.0 format
                            switch (count)
                            {
                            case 1:
                                pumpBytes[0] = (UINT8)(cbl_num << 4) | MIDI_CIN_SYSEX_END_1BYTE;
                                break;

                            case 2:
                            default:
                                pumpBytes[0] = (UINT8)(cbl_num << 4) | MIDI_CIN_SYSEX_END_2BYTE;
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                    umpWritePacket.wordCount++;
                }
                break;

            default:
                // Not handled so ignore
                numProcessed += umpPacket.wordCount;    // ignore this UMP packet as corrupted
                umpWritePacket.wordCount = 0;
            }

            if (umpWritePacket.wordCount)
            {
                numProcessed += umpPacket.wordCount;

                // If currently no write buffer, create one
                if (!pDeviceContext->DeviceWriteBuffer)
                {
                    // Create Request
                    status = WdfRequestCreate(
                        NULL,       // attributes
                        WdfUsbTargetPipeGetIoTarget(pipe),
                        &pDeviceContext->DeviceUSBWriteRequest    // retuest object
                    );
                    if (!NT_SUCCESS(status))
                    {
                        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                            "%!FUNC! Error creating request for USB Write with status: %!STATUS!", status);
                        goto DriverIoWriteExit;
                    }

                    WDF_OBJECT_ATTRIBUTES_INIT(&writeMemoryAttributes);
                    writeMemoryAttributes.ParentObject = pDeviceContext->DeviceUSBWriteRequest;

                    // Create Memory Object
                    status = WdfMemoryCreate(
                        &writeMemoryAttributes,
                        NonPagedPoolNx,
                        USBMIDI_POOLTAG,
                        pDeviceContext->MidiOutMaxSize,
                        &pDeviceContext->DeviceWriteMemory,
                        NULL
                    );
                    if (!NT_SUCCESS(status))
                    {
                        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                            "%!FUNC! could not create WdfMemory with status: %!STATUS!", status);
                        goto DriverIoWriteExit;
                    }
                    pDeviceContext->DeviceWriteBuffer = (PUCHAR)WdfMemoryGetBuffer(pDeviceContext->DeviceWriteMemory, NULL);
                }
                pWriteWords = (PUINT32)pDeviceContext->DeviceWriteBuffer;

                // Write into buffer
                for (int count = 0; count < umpWritePacket.wordCount; count++)
                {
                    pWriteWords[pDeviceContext->DeviceWriteBufferIndex++] = umpWritePacket.umpData.umpWords[count];
                }

                if ((pDeviceContext->DeviceWriteBufferIndex*sizeof(UINT32)) == pDeviceContext->MidiOutMaxSize)
                {
                    // Write to buffer
                    if (!USBMIDI2DriverSendToUSB(
                        pDeviceContext->DeviceUSBWriteRequest,
                        pDeviceContext->DeviceWriteMemory,
                        pipe,
                        pDeviceContext->DeviceWriteBufferIndex*sizeof(UINT32),
                        pDeviceContext,
                        true    // delete this request when complete
                    ))
                    {
                        goto DriverIoWriteExit;
                    }

                    // Indicate new buffer needed
                    pDeviceContext->DeviceWriteBuffer = NULL;
                    pDeviceContext->DeviceWriteBufferIndex = 0;
                }
            }
        }

        // Check if anything leftover to write to USB
        if (pDeviceContext->DeviceWriteBufferIndex && !isMoreData) // and there is not more data coming
        {
            // 
            // Write to buffer
            if (!USBMIDI2DriverSendToUSB(
                pDeviceContext->DeviceUSBWriteRequest,
                pDeviceContext->DeviceWriteMemory,
                pipe,
                pDeviceContext->DeviceWriteBufferIndex*sizeof(UINT32),
                pDeviceContext,
                true    // delete this request when complete
            ))
            {
                goto DriverIoWriteExit;
            }

            // Indicate new buffer needed
            pDeviceContext->DeviceWriteBuffer = NULL;
            pDeviceContext->DeviceWriteBufferIndex = 0;
        }
    }
    else if (pDeviceContext->UsbMIDIbcdMSC == MIDI_CS_BCD_MIDI2)
    {
        //
        // UMP device, just pass along
        //
        size_t transferPos = 0;
        size_t thisTransferSize = 0;

        while (transferPos < numBytes)
        {
            // If there is currently no USB packet being worked on, create a working space
            if (!pDeviceContext->DeviceWriteBuffer)
            {
                pDeviceContext->DeviceWriteBufferIndex = 0;

                // Create Request
                status = WdfRequestCreate(
                    NULL,       // attributes
                    WdfUsbTargetPipeGetIoTarget(pipe),
                    &pDeviceContext->DeviceUSBWriteRequest    // retuest object
                );
                if (!NT_SUCCESS(status))
                {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                        "%!FUNC! Error creating request for USB Write with status: %!STATUS!", status);
                    goto DriverIoWriteExit;
                }

                WDF_OBJECT_ATTRIBUTES_INIT(&writeMemoryAttributes);
                writeMemoryAttributes.ParentObject = pDeviceContext->DeviceUSBWriteRequest;

                // Create Memory Object
                status = WdfMemoryCreate(
                    &writeMemoryAttributes,
                    NonPagedPoolNx,
                    USBMIDI_POOLTAG,
                    pDeviceContext->MidiOutMaxSize,
                    &pDeviceContext->DeviceWriteMemory,
                    NULL
                );
                if (!NT_SUCCESS(status))
                {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                        "%!FUNC! could not create WdfMemory with status: %!STATUS!", status);
                    goto DriverIoWriteExit;
                }
                pDeviceContext->DeviceWriteBuffer = (PUCHAR)WdfMemoryGetBuffer(pDeviceContext->DeviceWriteMemory, NULL);
            }

            thisTransferSize = numBytes - transferPos;

            bool pushUSBTransfer = false;
            PUINT32 pWriteMem = (PUINT32)pDeviceContext->DeviceWriteBuffer;

            // Can we transfer current into buffer?
            if ((thisTransferSize + pDeviceContext->DeviceWriteBufferIndex*sizeof(UINT32)) <= pDeviceContext->MidiOutMaxSize)
            {
                // Copy necessary data into the buffer performing byte swap
                PUINT32 pReadMem = (PUINT32)&pBuffer[transferPos];
                for (size_t count = 0; count < (thisTransferSize / sizeof(UINT32)); count++)
                {
                    pWriteMem[pDeviceContext->DeviceWriteBufferIndex++] = pReadMem[count];     // USB is Little Endian format
                }
                transferPos += thisTransferSize;
            }
            else
            {
                pushUSBTransfer = true;
            }

            if (pushUSBTransfer || !isMoreData)
            {
                // Write to buffer
                if (!USBMIDI2DriverSendToUSB(
                    pDeviceContext->DeviceUSBWriteRequest,
                    pDeviceContext->DeviceWriteMemory,
                    pipe,
                    pDeviceContext->DeviceWriteBufferIndex * sizeof(UINT32),
                    pDeviceContext,
                    true    // delete this request when complete
                ))
                {
                    goto DriverIoWriteExit;
                }

                pDeviceContext->DeviceWriteBuffer = NULL;
                pDeviceContext->DeviceWriteBufferIndex = 0;
            }
        }
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "Unknown MIDI BCD Inteface Type.\n");
        goto DriverIoWriteExit;
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


// TODO: Put into dedicated library file
_Use_decl_annotations_
NONPAGED_CODE_SEG
BOOL
USBMIDI1ToUMP(
    PUINT32         usbMidi1Pkt,
    bool* pbIsInSysex,
    PUMP_PACKET     umpPkt
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
        case UMP_SYSTEM_UNDEFINED_F4:
        case UMP_SYSTEM_UNDEFINED_F5:
        case UMP_SYSTEM_UNDEFINED_F9:
        case UMP_SYSTEM_UNDEFINED_FD:
            code_index = MIDI_CIN_SYSEX_END_1BYTE;
            break;

        default:
            break;
        }
    }

    UINT8 firstByte = 1;
    UINT8 lastByte = 4;
    UINT8 copyPos;

    switch (code_index)
    {
    case MIDI_CIN_SYSEX_START: // or continue
 
        if (!*pbIsInSysex)
        {
            // SYSEX Start means first byte should be SYSEX start
            if (pBuffer[1] != SYSEX_START) return false;
            firstByte = 2;
            lastByte = 4;

            // As this is start of SYSEX, need to set status to indicate so and copy 2 bytes of data
            // as first byte of SYSEX_START
            umpPkt->umpData.umpBytes[1] = UMP_SYSEX7_START | 2;

            // Set that in SYSEX
            *pbIsInSysex = true;
        }
        else
        {
            firstByte = 1;
            lastByte = 4;

            // As this is in SYSEX, then need to indicate continue
            umpPkt->umpData.umpBytes[1] = UMP_SYSEX7_CONTINUE | 3;
        }

        // Capture Cable number
        umpPkt->umpData.umpBytes[0] = UMP_MT_DATA_64 | cbl_num;   // Message Type and group

        umpPkt->wordCount = 2;
        // Transfer in bytes
        copyPos = firstByte;
        for (UINT8 count = 2; count < 8; count++)
        {
            umpPkt->umpData.umpBytes[count] = (copyPos < lastByte)
                ? pBuffer[copyPos++] : 0x00;
        }
        break;

    case MIDI_CIN_SYSEX_END_1BYTE: // or single byte System Common
        // Determine if a system common
        if ( (pBuffer[1] & 0x80) // most significant bit set and not sysex ending
            && (pBuffer[1] != SYSEX_STOP))
        {
            umpPkt->umpData.umpBytes[0] = UMP_MT_SYSTEM | cbl_num;
            umpPkt->umpData.umpBytes[1] = pBuffer[1];
            firstByte = 1;
            lastByte = 1;
            goto COMPLETE_1BYTE;
        }

        umpPkt->umpData.umpBytes[0] = UMP_MT_DATA_64 | cbl_num;

        // Determine if complete based on if currently in SYSEX
        if (*pbIsInSysex)
        {
            if (pBuffer[1] != SYSEX_STOP) return false;
            umpPkt->umpData.umpBytes[1] = UMP_SYSEX7_END | 0;
            *pbIsInSysex = false; // we are done with SYSEX
            firstByte = 1;
            lastByte = 1;
        }
        else
        {
            // should not get here
            return false;
        }

COMPLETE_1BYTE:
        umpPkt->wordCount = 2;
        // Transfer in bytes
        copyPos = firstByte;
        for (UINT8 count = 2; count < 8; count++)
        {
            umpPkt->umpData.umpBytes[count] = (copyPos < lastByte)
                ? pBuffer[copyPos++] : 0x00;
        }
        break;

    case MIDI_CIN_SYSEX_END_2BYTE:
        umpPkt->umpData.umpBytes[0] = UMP_MT_DATA_64 | cbl_num;

        // Determine if complete based on if currently in SYSEX
        if (*pbIsInSysex)
        {
            if (pBuffer[2] != SYSEX_STOP) return false;
            umpPkt->umpData.umpBytes[1] = UMP_SYSEX7_END | 1;
            *pbIsInSysex = false; // we are done with SYSEX
            firstByte = 1;
            lastByte = 2;
        }
        else
        {
            umpPkt->umpData.umpBytes[1] = UMP_SYSEX7_COMPLETE | 0;
            *pbIsInSysex = false; // we are done with SYSEX
            firstByte = 1;
            lastByte = 1;
        }

        umpPkt->wordCount = 2;
        // Transfer in bytes
        copyPos = firstByte;
        for (UINT8 count = 2; count < 8; count++)
        {
            umpPkt->umpData.umpBytes[count] = (copyPos < lastByte)
                ? pBuffer[copyPos++] : 0x00;
        }
        break;

    case MIDI_CIN_SYSEX_END_3BYTE:
        umpPkt->umpData.umpBytes[0] = UMP_MT_DATA_64 | cbl_num;

        // Determine if complete based on if currently in SYSEX
        if (*pbIsInSysex)
        {
            if (pBuffer[3] != SYSEX_STOP) return false;
            umpPkt->umpData.umpBytes[1] = UMP_SYSEX7_END | 2;
            *pbIsInSysex = false; // we are done with SYSEX
            firstByte = 1;
            lastByte = 3;
        }
        else
        {
            if (pBuffer[1] != SYSEX_START || pBuffer[3] != SYSEX_STOP) return false;
            umpPkt->umpData.umpBytes[1] = UMP_SYSEX7_COMPLETE | 1;
            *pbIsInSysex = false; // we are done with SYSEX
            firstByte = 2;
            lastByte = 3;
        }

        umpPkt->wordCount = 2;
        // Transfer in bytes
        copyPos = firstByte;
        for (UINT8 count = 2; count < 8; count++)
        {
            umpPkt->umpData.umpBytes[count] = (copyPos < lastByte)
                ? pBuffer[copyPos++] : 0x00;
        }
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