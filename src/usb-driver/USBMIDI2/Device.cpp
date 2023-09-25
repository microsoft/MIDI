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

    device.c - Device handling events for example driver.

Abstract:

   This file contains the device entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "Pch.h"
#include "ump.h"

#include "Trace.h"
#include "Device.tmh"

UNICODE_STRING g_RegistryPath = {0};      // This is used to store the registry settings path for the driver

_Use_decl_annotations_
PAGED_CODE_SEG
NTSTATUS
CopyRegistrySettingsPath(
    _In_ PUNICODE_STRING RegistryPath
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
    _In_    WDFDRIVER        Driver,
    _Inout_ PWDFDEVICE_INIT  DeviceInit
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
    _In_ WDFDEVICE      Device,
    _In_ WDFCMRESLIST   ResourceList,
    _In_ WDFCMRESLIST   ResourceListTranslated
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
                "WdfUsbTargetDeviceCreateWithParameters failed 0x%x", status);
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
            "USBUMPDriverSelectInterface failed 0x%x", status);
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
    _In_ WDFDEVICE      Device,
    _In_ WDFCMRESLIST   ResourceListTranslated
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

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return STATUS_SUCCESS;
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

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");
    
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

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

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
NTSTATUS
SetPowerPolicy(
    _In_ WDFDEVICE Device
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

PAGED_CODE_SEG
VOID
EvtDeviceContextCleanup(
    _In_ WDFOBJECT      WdfDevice
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

PAGED_CODE_SEG
NTSTATUS
USBMIDI2DriverSelectInterface(
    _In_ WDFDEVICE    Device
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
    WDFUSBPIPE                              pipe;
    PWDF_USB_INTERFACE_SETTING_PAIR         pSettingPairs = NULL;
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS     configParams;
    WDF_OBJECT_ATTRIBUTES                   objectAttributes;
    PUSB_STRING_DESCRIPTOR                  pTempBuffer = NULL;
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
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Configuration Descriptor Error fetching size.\n");
        return(status);
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&deviceConfigAttrib);
    deviceConfigAttrib.ParentObject = pDeviceContext->UsbDevice;
    status = WdfMemoryCreate(
        &deviceConfigAttrib,
        NonPagedPool,
        USBMIDI_POOLTAG,
        configurationDescriptorSize,
        &pDeviceContext->DeviceConfigDescriptorMemory,
        (PVOID*)&pConfigurationDescriptor
    );
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error allocating memory for configuration descriptor.\n");
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
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error loading configuration descriptor.\n");
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
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting Serial Number string size.\n");
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
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error allocating memory for Serial Number.\n");
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
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting Serial Number string.\n");
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
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting Manufacturer string size.\n");
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
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error allocating memory for Manufacturer.\n");
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
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Error getting Manufacturer string.\n");
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
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Could not obtain Friendly Name String\n");
    }

    //
    // Select target interfaces
    //
    numInterfaces = WdfUsbTargetDeviceGetNumInterfaces(pDeviceContext->UsbDevice);
    // Setup store for setting pairs for use later
    if (numInterfaces >= 2)
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
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "Insufficient Resources\n");
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    // We expect at least two interfaces (and only currently support two)
    // an Audio Control interface and the MIDI interface
    if (numInterfaces < 2)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "Number of interfaces, %d, less than expected.\n", numInterfaces);
        status = STATUS_SEVERITY_ERROR;
        goto SelectExit;
    }

    // Load and configure pipes for first interface which should be the control interface
    pDeviceContext->UsbControlInterface = WdfUsbTargetDeviceGetInterface(
        pDeviceContext->UsbDevice,
        0
    );

    // Check that is control interface
    WdfUsbInterfaceGetDescriptor(
        pDeviceContext->UsbControlInterface,
        0,
        &interfaceDescriptor
    );
    if (interfaceDescriptor.bInterfaceClass != 1 /*AUDIO*/
        || interfaceDescriptor.bInterfaceSubClass != 1 /*AUDIO_CONTROL*/)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Device detected does not have AUDIO_CONTROL interface as expected\n");
        status = STATUS_SEVERITY_ERROR;
        goto SelectExit;
    }

    // Setup setting pairs for control interface
    pSettingPairs[0].UsbInterface = pDeviceContext->UsbControlInterface;
    pSettingPairs[0].SettingIndex = 0;

    // Check that next interface is MIDI Streaming Interface
    pDeviceContext->UsbMIDIStreamingInterface = WdfUsbTargetDeviceGetInterface(
        pDeviceContext->UsbDevice,
        1
    );
    // First look if an alternate interface available
    WdfUsbInterfaceGetDescriptor(
        pDeviceContext->UsbMIDIStreamingInterface,
        1,  //alternate interface 1 - see USB Device Class Definition for MIDI Devices, Version 2.0 - Section 3.1
        &interfaceDescriptor
    );
    // If alternate interface 1 does not exist, then the length will be null
    if (interfaceDescriptor.bLength)
    {
        if (interfaceDescriptor.bInterfaceClass != 1 /*AUDIO*/
            || interfaceDescriptor.bInterfaceSubClass != 3 /*MIDI STREAMING*/
            || interfaceDescriptor.bInterfaceProtocol != 0) /*UNUSED*/
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                "Device Class/SubClass/Protocol not 1/3/0 as expected.\n");
            status = STATUS_SEVERITY_ERROR;
            goto SelectExit;
        }
        pDeviceContext->UsbMIDIStreamingAlt = 1;    // store that alternate interface
    }
    else
    {
        // As no additional alternate interface, work to select default 0
        WdfUsbInterfaceGetDescriptor(
            pDeviceContext->UsbMIDIStreamingInterface,
            0, //alternate interface 0, default - see USB Device Class Definition for MIDI Devices
            &interfaceDescriptor
        );
        if (!interfaceDescriptor.bLength /*error fetching descriptor*/
            || interfaceDescriptor.bInterfaceClass != 1 /*AUDIO*/
            || interfaceDescriptor.bInterfaceSubClass != 3 /*MIDI STREAMING*/
            || interfaceDescriptor.bInterfaceProtocol != 0) /*UNUSED*/
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                "Device Class/SubClass/Protocol not 1/3/0 as expected.\n");
            status = STATUS_SEVERITY_ERROR;
            goto SelectExit;
        }
        pDeviceContext->UsbMIDIStreamingAlt = 0;    // store that original interface
    }
    // Setup settings pairs for MIDI streaming interface
    pSettingPairs[1].UsbInterface = pDeviceContext->UsbMIDIStreamingInterface;
    pSettingPairs[1].SettingIndex = pDeviceContext->UsbMIDIStreamingAlt;

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

PAGED_CODE_SEG
NTSTATUS
USBMIDI2DriverEnumeratePipes(
    _In_ WDFDEVICE    Device
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
    PWDF_USB_INTERFACE_SETTING_PAIR         pSettingPairs = NULL;
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
}

NONPAGED_CODE_SEG
VOID USBMIDI2DriverEvtReadComplete(
    _In_    WDFUSBPIPE Pipe,
    _In_    WDFMEMORY  Buffer,
    _In_    size_t     NumBytesTransferred,
    _In_    WDFCONTEXT Context
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
    WDFMEMORY           workingBuffer = NULL;
    size_t              workingBufferSize;
    PUINT8              pWorkingBuffer;
    UMP_PACKET          umpPacket;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

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

        pReceivedBuffer = (PUCHAR)WdfMemoryGetBuffer(Buffer, NULL);

        // If Alt setting 0 then we need to process into URB packets for USB MIDI 1.0 to UMP
        if (!pDeviceContext->UsbMIDIStreamingAlt)
        {
            // Create a working buffer
            workingBufferSize = ((NumBytesTransferred * 2) / sizeof(UINT32)) * sizeof(UINT32);
            // For now assume that could wind up with twice
            // size in converting from USB MIDI 1.0 to UMP
            // making sure that overall size even on 32 bit words.
            status = WdfMemoryCreate(
                NULL,       // attributes
                NonPagedPool,
                NULL,       // Pool Tag
                workingBufferSize,
                &workingBuffer,
                NULL
            );

            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                    "Error creating working buffer for USB MIDI 1.0 to UMP conversion.\n");
                goto ReadCompleteExit;
            }
            pWorkingBuffer = (PUINT8)WdfMemoryGetBuffer(workingBuffer, NULL);

            // We process into working buffer based on UINT32s
            UINT16 numAvail = workingBufferSize / sizeof(UINT32);
            UINT16 numIndex = 0;

            // Process received data as UINT32 Packets
            for (int byteCount = 0; byteCount < NumBytesTransferred; byteCount += 4)
            {
                // Get current packet buffer
                PUINT8 pBuffer = &pReceivedBuffer[byteCount];

                // Determine packet cable number from group
                UINT8 cbl_num = (pBuffer[0] & 0xf0) >> 4;

                // ***Process data into MIDI1 data format***
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
                    // Needs more space, confirm available
                    if (!(numIndex + 1 < numAvail))
                    {
                        continue;
                    }

                    umpPacket.umpData.umpBytes[0] = UMP_MT_DATA_64 | cbl_num;   // Message Type and group

                    // As this is start of SYSEX, need to set status to indicate so and copy 3 bytes of data
                    umpPacket.umpData.umpBytes[1] = UMP_SYSEX7_START | 3;

                    // Set that in SYSEX
                    pDeviceContext->midi1IsInSysex[cbl_num] = true;

                    // Copy in rest of data
                    for (int count = 1; count < 4; count++)
                    {
                        umpPacket.umpData.umpBytes[count + 1] = pBuffer[count];
                    }
                    // Pad rest of data
                    umpPacket.umpData.umpBytes[5] = 0x00;
                    umpPacket.umpData.umpBytes[6] = 0x00;
                    umpPacket.umpData.umpBytes[7] = 0x00;

                    umpPacket.wordCount = 2;
                    break;

                case MIDI_CIN_SYSEX_END_1BYTE: // or single byte System Common
                    // Determine if a system common
                    if ((pBuffer[1] & 0x80) // most significant bit set and not sysex
                        && (pBuffer[1] != MIDI_STATUS_SYSEX_END))
                    {
                        umpPacket.umpData.umpBytes[0] = UMP_MT_SYSTEM | cbl_num;
                        umpPacket.umpData.umpBytes[1] = pBuffer[1];
                        umpPacket.umpData.umpBytes[2] = 0x00;
                        umpPacket.umpData.umpBytes[3] = 0x00;
                        umpPacket.wordCount = 1;
                        break;
                    }

                    // Sysex so 64 bit message Needs more space, confirm available
                    if (!(numIndex + 1 < numAvail))
                    {
                        // not enough space need to error out or add more
                        continue;
                    }

                    umpPacket.umpData.umpBytes[0] = UMP_MT_DATA_64 | cbl_num;

                    // Determine if complete based on if currently in SYSEX
                    if (pDeviceContext->midi1IsInSysex[cbl_num])
                    {
                        umpPacket.umpData.umpBytes[1] = UMP_SYSEX7_END | 1;
                        pDeviceContext->midi1IsInSysex[cbl_num] = false; // we are done with SYSEX
                    }
                    else
                    {
                        umpPacket.umpData.umpBytes[1] = UMP_SYSEX7_COMPLETE | 1;
                    }

                    // Copy in the data, assumes the original USB MIDI 1.0 data has padded data
                    for (int count = 1; count < 4; count++)
                    {
                        umpPacket.umpData.umpBytes[count + 1] = pBuffer[count];
                    }

                    umpPacket.wordCount = 2;
                    break;

                case MIDI_CIN_SYSEX_END_2BYTE:
                    // Needs more space, confirm available
                    if (!(numIndex + 1 < numAvail))
                    {
                        // not enough space need to error out or add more
                        continue;
                    }

                    umpPacket.umpData.umpBytes[0] = UMP_MT_DATA_64 | cbl_num;

                    // Determine if complete based on if currently in SYSEX
                    if (pDeviceContext->midi1IsInSysex[cbl_num])
                    {
                        umpPacket.umpData.umpBytes[1] = UMP_SYSEX7_END | 2;
                        pDeviceContext->midi1IsInSysex[cbl_num] = false; // we are done with SYSEX
                    }
                    else
                    {
                        umpPacket.umpData.umpBytes[1] = UMP_SYSEX7_COMPLETE | 2;
                    }

                    // Copy in the data, assumes the original USB MIDI 1.0 data has padded data
                    for (int count = 1; count < 4; count++)
                    {
                        umpPacket.umpData.umpBytes[count + 1] = pBuffer[count];
                    }
                    // Pad rest of data
                    umpPacket.umpData.umpBytes[6] = 0x00;
                    umpPacket.umpData.umpBytes[7] = 0x00;

                    umpPacket.wordCount = 2;
                    break;

                case MIDI_CIN_SYSEX_END_3BYTE:
                    // Needs more space, confirm available
                    if (!(numIndex + 1 < numAvail))
                    {
                        // not enough space need to error out or add more
                        continue;
                    }

                    umpPacket.umpData.umpBytes[0] = UMP_MT_DATA_64 | cbl_num;

                    // Determine if complete based on if currently in SYSEX
                    if (pDeviceContext->midi1IsInSysex[cbl_num])
                    {
                        umpPacket.umpData.umpBytes[1] = UMP_SYSEX7_END | 3;
                        pDeviceContext->midi1IsInSysex[cbl_num] = false; // we are done with SYSEX
                    }
                    else
                    {
                        umpPacket.umpData.umpBytes[1] = UMP_SYSEX7_COMPLETE | 3;
                    }

                    // Copy in the data, assumes the original USB MIDI 1.0 data has padded data
                    for (int count = 1; count < 4; count++)
                    {
                        umpPacket.umpData.umpBytes[count + 1] = pBuffer[count];
                    }
                    // Pad rest of data
                    umpPacket.umpData.umpBytes[6] = 0x00;
                    umpPacket.umpData.umpBytes[7] = 0x00;

                    umpPacket.wordCount = 2;
                    break;

                    // MIDI1 Channel Voice Messages
                case MIDI_CIN_NOTE_ON:
                case MIDI_CIN_NOTE_OFF:
                case MIDI_CIN_POLY_KEYPRESS:
                case MIDI_CIN_CONTROL_CHANGE:
                case MIDI_CIN_PROGRAM_CHANGE:
                case MIDI_CIN_CHANNEL_PRESSURE:
                case MIDI_CIN_PITCH_BEND_CHANGE:
                    umpPacket.umpData.umpBytes[0] = UMP_MT_MIDI1_CV | cbl_num; // message type 2
                    pDeviceContext->midi1IsInSysex[cbl_num] = false; // ensure we end any current sysex packets, other layers need to handle error

                    // Copy in rest of data
                    for (int count = 1; count < 4; count++)
                    {
                        umpPacket.umpData.umpBytes[count] = pBuffer[count];
                    }

                    umpPacket.wordCount = 1;
                    break;

                case MIDI_CIN_SYSCOM_2BYTE:
                case MIDI_CIN_SYSCOM_3BYTE:
                    umpPacket.umpData.umpBytes[0] = UMP_MT_SYSTEM | cbl_num;
                    for (int count = 1; count < 4; count++)
                    {
                        umpPacket.umpData.umpBytes[count] = pBuffer[count];
                    }
                    umpPacket.wordCount = 1;
                    break;

                case MIDI_CIN_MISC:
                case MIDI_CIN_CABLE_EVENT:
                    // These are reserved for future use and will not be translated, drop data with no processing
                default:
                    // Error or not handled
                    break;
                }

                // write to packets
                PUINT32 pkts = (UINT32*)pWorkingBuffer;
                for (int pktCount = 0; pktCount < umpPacket.wordCount; pktCount++)
                {
                    pkts[numIndex++] = umpPacket.umpData.umpWords[pktCount];
                }
            }
        }
        else
        {
            // Send Memory to Read Queue
#if 0
            if (!USBUMPDriverFillReadQueue(
                (PUINT32)pReceivedBuffer,
                NumBytesTransferred / sizeof(UINT32),
                pDeviceContext
            ))
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                    "Error submitting to read queue prep buffer.\n");
                goto ReadCompleteExit;
            }
#endif
        }
    }

ReadCompleteExit:
    if (workingBuffer)
    {
        // if we created a working buffer, be sure to get rid of it
        WdfObjectDelete(workingBuffer);
    }

    // The continuous reader takes care of completion of event and clearing buffer

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return;
}

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