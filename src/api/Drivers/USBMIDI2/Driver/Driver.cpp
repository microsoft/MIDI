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

    Driver.cpp

Abstract:

    This file contains the driver entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "Pch.h"

#include "Driver.h"

#include "Trace.h"
#include "Driver.tmh"

extern wil::fast_mutex_with_critical_region *g_MidiInLock;

//_Use_decl_annotations_
PAGED_CODE_SEG
void
DriverUnload(
    WDFDRIVER Driver
    )
    /*++

    Routine Description:
        DriverUnload removes any allocated elements for the driver not handled
        by bag or WDF. Also closes off debug tracing.

    Parameters Description:

        Driver - the instance of the driver.

    Return Value:

NOTE: Consider putting globals into context and using context cleanup

    --*/
{
    PAGED_CODE(); 

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    if (!Driver)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "No driver instance supplied.");
        goto exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    WPP_CLEANUP(WdfDriverWdmGetDriverObject(Driver));

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
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath
    )
    /*++

    Routine Description:
        DriverEntry initializes the driver and is the first routine called by the
        system after the driver is loaded. DriverEntry specifies the other entry
        points in the function driver, suopych as EvtDevice and DriverUnload.

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
    
    //
    // Initialize WPP Tracking
    //
    WPP_INIT_TRACING(DriverObject, RegistryPath);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    status = CopyRegistrySettingsPath(RegistryPath);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "CopyRegistrySettingsPath failed %!STATUS!", status);
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
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDriverCreate failed %!STATUS!", status);
        goto exit;
    }

    //
    // Post init.
    //
    ACX_DRIVER_CONFIG_INIT(&acxCfg);

    status = AcxDriverInitialize(driver, &acxCfg);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "AcxDriverInitialize failed %!STATUS!", status);
        goto exit;
    }

    if (!g_MidiInLock)
    {
        g_MidiInLock = new (POOL_FLAG_NON_PAGED) wil::fast_mutex_with_critical_region();
    }

exit:
    if (!NT_SUCCESS(status))
    {
        WPP_CLEANUP(DriverObject);

        if (g_RegistryPath.Buffer != nullptr)
        {
            ExFreePool(g_RegistryPath.Buffer);
            RtlZeroMemory(&g_RegistryPath, sizeof(g_RegistryPath));
        }
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}
