/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    Driver.cpp

Abstract:

    Min MIDI 2 Driver.

Environment:

    Kernel mode only

--*/

#include "Pch.h"

extern wil::fast_mutex_with_critical_region *g_MidiInLock;

PAGED_CODE_SEG
void
DriverUnload(
    _In_ WDFDRIVER Driver
    )
{
    PAGED_CODE(); 

    if (!Driver)
    {
        ASSERT(FALSE);
        goto exit;
    }

    //WPP_CLEANUP(WdfDriverWdmGetDriverObject(Driver));

    if (g_RegistryPath.Buffer != nullptr)
    {
        ExFreePool(g_RegistryPath.Buffer);
        RtlZeroMemory(&g_RegistryPath, sizeof(g_RegistryPath));
    }

    if (g_MidiInLock)
    {
        delete g_MidiInLock;
        g_MidiInLock = nullptr;
    }

exit:
    return;
}

_Use_decl_annotations_
INIT_CODE_SEG
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:
    DriverEntry initializes the driver and is the first routine called by the
    system after the driver is loaded.

Parameters Description:

    DriverObject - represents the instance of the function driver that is loaded
    into memory. DriverEntry must initialize members of DriverObject before it
    returns to the caller. DriverObject is allocated by the system before the
    driver is loaded, and it is released by the system after the system unloads
    the function driver from memory.

    RegistryPath - represents the driver specific path in the Registry.
    The function driver can use the path to store driver related data between
    reboots. The path does not store hardware instance specific data.

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise.

--*/
{
    WDF_DRIVER_CONFIG           wdfCfg;
    ACX_DRIVER_CONFIG           acxCfg;
    WDFDRIVER                   driver;
    NTSTATUS                    status;
    WDF_OBJECT_ATTRIBUTES       attributes;
    
    PAGED_CODE(); 
    //WPP_INIT_TRACING(DriverObject, RegistryPath);

    status = CopyRegistrySettingsPath(RegistryPath);
    if (!NT_SUCCESS(status))
    {
        ASSERT(FALSE);
        goto exit;
    }

    //
    // Initiialize driver config to control the attributes that
    // are global to the driver. Note that framework by default
    // provides a driver unload routine. If you create any resources
    // in the DriverEntry and want to be cleaned in driver unload,
    // you can override that by manually setting the EvtDriverUnload in the
    // config structure. In general xxx_CONFIG_INIT macros are provided to
    // initialize most commonly used members.
    //

    WDF_DRIVER_CONFIG_INIT(&wdfCfg, EvtBusDeviceAdd);
    wdfCfg.EvtDriverUnload = DriverUnload;

    //
    // Add a driver context. (for illustration purposes only).
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DRIVER_CONTEXT);

    //
    // Create a framework driver object to represent our driver.
    //
    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        &attributes,            // Driver Attributes
        &wdfCfg,                // Driver Config Info
        &driver                 // hDriver
        );

    if (!NT_SUCCESS(status)) {
        ASSERT(FALSE);
        goto exit;
    }

    //
    // Post init.
    //
    ACX_DRIVER_CONFIG_INIT(&acxCfg);

    status = AcxDriverInitialize(driver, &acxCfg);
    if (!NT_SUCCESS(status)) {
        ASSERT(FALSE);
        goto exit;
    }

    if (!g_MidiInLock)
    {
        g_MidiInLock = new (POOL_FLAG_NON_PAGED) wil::fast_mutex_with_critical_region();
    }

exit:
    if (!NT_SUCCESS(status))
    {
        //WPP_CLEANUP(DriverObject);

        if (g_RegistryPath.Buffer != nullptr)
        {
            ExFreePool(g_RegistryPath.Buffer);
            RtlZeroMemory(&g_RegistryPath, sizeof(g_RegistryPath));
        }
    }

    return status;
}
