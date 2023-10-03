/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Device.cpp

Abstract:

    Plug and Play module. This file contains routines to handle pnp requests.

Environment:

    Kernel mode

--*/

#include "Pch.h"

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

    //
    // Allow ACX to add any pre-requirement it needs on this device.
    //
    ACX_DEVICEINIT_CONFIG_INIT(&devInitCfg);
    status = AcxDeviceInitInitialize(DeviceInit, &devInitCfg);
    if (!NT_SUCCESS(status)) {
        ASSERT(FALSE);
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
        ASSERT(FALSE);
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
        ASSERT(FALSE);
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
        ASSERT(FALSE);
        goto exit;
    }

    status = STATUS_SUCCESS;

exit:
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
    hardware ready to use.  

Arguments:

    Device - handle to a device

Return Value:

    NT status value

--*/
{
    NTSTATUS                status;
    PDEVICE_CONTEXT         devCtx;

    UNREFERENCED_PARAMETER(ResourceList);
    UNREFERENCED_PARAMETER(ResourceListTranslated);

    PAGED_CODE();

    devCtx = GetDeviceContext(Device);
    ASSERT(devCtx != nullptr);

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

    device = (WDFDEVICE)WdfDevice;
    devCtx = GetDeviceContext(device);
    ASSERT(devCtx != nullptr);

    if (devCtx->Midi)
    {
        ; // TODO: CircuitCleanup(devCtx->Midi);
    }
}

