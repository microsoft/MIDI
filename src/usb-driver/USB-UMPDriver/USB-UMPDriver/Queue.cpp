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

    queue.c

Abstract:

    This file contains the queue entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "queue.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, USBUMPDriverQueueInitialize)
#endif

NTSTATUS
USBUMPDriverQueueInitialize(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:


     The I/O dispatch callbacks for the frameworks device object
     are configured in this function.

     A single default I/O Queue is configured for parallel request
     processing, and a driver context memory allocation is created
     to hold our structure QUEUE_CONTEXT.

Arguments:

    Device - Handle to a framework device object.

Return Value:

    VOID

--*/
{
    WDFQUEUE queue;
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG    queueConfig;
    PDEVICE_CONTEXT         pDeviceContext;
    PAGED_CODE();
    
    pDeviceContext = DeviceGetContext(Device);

    //
    // Configure a default queue so that requests that are not
    // configure-fowarded using WdfDeviceConfigureRequestDispatching to goto
    // other queues get dispatched here.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
         &queueConfig,
        WdfIoQueueDispatchParallel
        );

    queueConfig.EvtIoDeviceControl = USBUMPDriverEvtIoDeviceControl;
    queueConfig.EvtIoStop = USBUMPDriverEvtIoStop;

    status = WdfIoQueueCreate(
                 Device,
                 &queueConfig,
                 WDF_NO_OBJECT_ATTRIBUTES,
                 &queue
                 );

    if( !NT_SUCCESS(status) ) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoQueueCreate failed %!STATUS!", status);
        return status;
    }


    //
    // Configure a write queue to queue UMP formatted data to be sent
    // to USB device. This queue should be set to output of Kernel Streaming
    // freamework.
    //
    WDF_IO_QUEUE_CONFIG_INIT(
        &queueConfig,
        WdfIoQueueDispatchSequential
    );
    queueConfig.EvtIoWrite = USBUMPDriverEvtIoWrite;
    queueConfig.EvtIoStop = USBUMPDriverEvtIoStop;

    status = WdfIoQueueCreate(
        Device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue
    );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoQueueCreate failed %!STATUS!", status);
        return status;
    }

    status = WdfDeviceConfigureRequestDispatching(
        Device,
        queue,
        WdfRequestTypeWrite
    );
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
            "WdfDeviceConfigureDispatching failed 0x%x\n", status);
        return status;
    }


//
// Configure a read queue to queue to place read or converted UMP data
// from USB device. This queue should be set to input of Kernel Streaming
// driver.
//
    WDF_IO_QUEUE_CONFIG_INIT(
        &queueConfig,
        WdfIoQueueDispatchSequential
    );
    queueConfig.EvtIoRead = USBUMPDriverEvtIoRead;
    queueConfig.EvtIoStop = USBUMPDriverEvtIoStop;

    status = WdfIoQueueCreate(
        Device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue
    );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoQueueCreate failed %!STATUS!", status);
        return status;
    }

    status = WdfDeviceConfigureRequestDispatching(
        Device,
        queue,
        WdfRequestTypeRead
    );
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
            "WdfDeviceConfigureDispatching failed 0x%x\n", status);
        return status;
    }


    return status;
}

VOID
USBUMPDriverEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    This event is invoked when the framework receives IRP_MJ_DEVICE_CONTROL request.

Arguments:

    Queue -  Handle to the framework queue object that is associated with the
             I/O request.

    Request - Handle to a framework request object.

    OutputBufferLength - Size of the output buffer in bytes

    InputBufferLength - Size of the input buffer in bytes

    IoControlCode - I/O control code.

Return Value:

    VOID

--*/
{
    WDFDEVICE           Device;
    PDEVICE_CONTEXT     pDeviceContext;
    NTSTATUS            status = NULL;
    size_t              length = 0;
    size_t              requestLength = 0;
    PVOID               pMemBuffer;
    PVOID               ioBuffer;
    size_t              bufLength;
    UINT32              deviceType;

    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_QUEUE, 
                "%!FUNC! Queue 0x%p, Request 0x%p OutputBufferLength %d InputBufferLength %d IoControlCode %d", 
                Queue, Request, (int) OutputBufferLength, (int) InputBufferLength, IoControlCode);

    //
    // Initialize variables for use
    //
    Device = WdfIoQueueGetDevice(Queue);
    pDeviceContext = DeviceGetContext(Device);
    deviceType = (UINT32)pDeviceContext->UsbMIDIStreamingAlt;

    //
    // Perform requested action
    //
    switch (IoControlCode)
    {
    case IOCTL_USBUMPDRIVER_GET_CONFIG_DESCRIPTOR:
        if (pDeviceContext->DeviceConfigDescriptorMemory) {

            pMemBuffer = WdfMemoryGetBuffer(pDeviceContext->DeviceConfigDescriptorMemory, &length);

            status = WdfRequestRetrieveOutputBuffer(Request, length, &ioBuffer, &bufLength);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE,
                    "%!FUNC! Queue 0x%p, WdfReqesutRetriveOutputBuffer failed!\n", Queue);
                break;
            }

            RtlCopyMemory(ioBuffer,
                pMemBuffer,
                length);

            requestLength = length;
            status = STATUS_SUCCESS;
        }
        else {
            status = STATUS_INVALID_DEVICE_STATE;
        }
        break;

    case IOCTL_USBUMPDRIVER_GET_MFGNAME:
        if (pDeviceContext->DeviceManfMemory) {

            pMemBuffer = WdfMemoryGetBuffer(pDeviceContext->DeviceManfMemory, &length);

            status = WdfRequestRetrieveOutputBuffer(Request, length, &ioBuffer, &bufLength);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE,
                    "%!FUNC! Queue 0x%p, WdfReqesutRetriveOutputBuffer failed!\n", Queue);
                break;
            }

            RtlCopyMemory(ioBuffer,
                pMemBuffer,
                length);

            requestLength = length;
            status = STATUS_SUCCESS;
        }
        else {
            status = STATUS_INVALID_DEVICE_STATE;
        }
        break;

    case IOCTL_USBUMPDRIVER_GET_DEVICENAME:
        if (pDeviceContext->DeviceNameMemory) {

            pMemBuffer = WdfMemoryGetBuffer(pDeviceContext->DeviceNameMemory, &length);

            status = WdfRequestRetrieveOutputBuffer(Request, length, &ioBuffer, &bufLength);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE,
                    "%!FUNC! Queue 0x%p, WdfReqesutRetriveOutputBuffer failed!\n", Queue);
                break;
            }

            RtlCopyMemory(ioBuffer,
                pMemBuffer,
                length);

            requestLength = length;
            status = STATUS_SUCCESS;
        }
        else {
            status = STATUS_INVALID_DEVICE_STATE;
        }
        break;

    case IOCTL_USBUMPDRIVER_GET_SERIALNUM:
        if (pDeviceContext->DeviceSNMemory) {
            pMemBuffer = WdfMemoryGetBuffer(pDeviceContext->DeviceSNMemory, &length);

            status = WdfRequestRetrieveOutputBuffer(Request, length, &ioBuffer, &bufLength);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE,
                    "%!FUNC! Queue 0x%p, WdfReqesutRetriveOutputBuffer failed!\n", Queue);
                break;
            }

            RtlCopyMemory(ioBuffer,
                pMemBuffer,
                length);

            requestLength = length;
            status = STATUS_SUCCESS;
        }
        else {
            status = STATUS_INVALID_DEVICE_STATE;
        }
        break;

    case IOCTL_USBUMPDRIVER_GET_GTBDUMP:
        status = WdfRequestRetrieveOutputBuffer(Request, length, &ioBuffer, &bufLength);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE,
                "%!FUNC! Queue 0x%p, WdfReqesutRetriveOutputBuffer failed!\n", Queue);
            break;
        }

        if (pDeviceContext->DeviceGTBMemory) {
            pMemBuffer = WdfMemoryGetBuffer(pDeviceContext->DeviceGTBMemory, &length);
            RtlCopyMemory(ioBuffer,
                pMemBuffer,
                length);

            requestLength = length;
            status = STATUS_SUCCESS;
        }
        else {
            status = STATUS_INVALID_DEVICE_STATE;
        }
        break;

    case IOCTL_USBUMPDRIVER_GET_DEVICETYPE:
        pMemBuffer = (PVOID)&deviceType;
        length = sizeof(UINT32);

        status = WdfRequestRetrieveOutputBuffer(Request, length, &ioBuffer, &bufLength);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE,
                "%!FUNC! Queue 0x%p, WdfReqesutRetriveOutputBuffer failed!\n", Queue);
            break;
        }

        RtlCopyMemory(ioBuffer,
            pMemBuffer,
            length);

        requestLength = length;
        status = STATUS_SUCCESS;
        break;

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    WdfRequestCompleteWithInformation(Request, status, requestLength);

    return;
}

VOID
USBUMPDriverEvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
)
/*++

Routine Description:

    This event is invoked for a power-managed queue before the device leaves the working state (D0).

Arguments:

    Queue -  Handle to the framework queue object that is associated with the
             I/O request.

    Request - Handle to a framework request object.

    ActionFlags - A bitwise OR of one or more WDF_REQUEST_STOP_ACTION_FLAGS-typed flags
                  that identify the reason that the callback function is being called
                  and whether the request is cancelable.

Return Value:

    VOID

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_QUEUE, 
                "%!FUNC! Queue 0x%p, Request 0x%p ActionFlags %d", 
                Queue, Request, ActionFlags);

    //
    // In most cases, the EvtIoStop callback function completes, cancels, or postpones
    // further processing of the I/O request.
    //
    // Typically, the driver uses the following rules:
    //
    // - If the driver owns the I/O request, it either postpones further processing
    //   of the request and calls WdfRequestStopAcknowledge, or it calls WdfRequestComplete
    //   with a completion status value of STATUS_SUCCESS or STATUS_CANCELLED.
    //  
    //   The driver must call WdfRequestComplete only once, to either complete or cancel
    //   the request. To ensure that another thread does not call WdfRequestComplete
    //   for the same request, the EvtIoStop callback must synchronize with the driver's
    //   other event callback functions, for instance by using interlocked operations.
    //
    // - If the driver has forwarded the I/O request to an I/O target, it either calls
    //   WdfRequestCancelSentRequest to attempt to cancel the request, or it postpones
    //   further processing of the request and calls WdfRequestStopAcknowledge.
    //
    // A driver might choose to take no action in EvtIoStop for requests that are
    // guaranteed to complete in a small amount of time. For example, the driver might
    // take no action for requests that are completed in one of the driver’s request handlers.
    //

    return;
}
