// Copyright (c) Microsoft Corporation. All rights reserved.
#include <windows.h>
#include <cguid.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <devioctl.h>
#include <ks.h>
#include <ksmedia.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>

#include <Devpkey.h>
#include "MidiKsDef.h"
#include "MidiKsCommon.h"

_Use_decl_annotations_
HRESULT 
SyncIoctl
(
    HANDLE  hHandle,
    ULONG   ulIoctl,
    PVOID   pvInBuffer,
    ULONG   cbInBuffer,
    PVOID   pvOutBuffer,
    ULONG   cbOutBuffer,
    PULONG  pulBytesReturned
)
{
    OVERLAPPED overlapped;
    wil::unique_handle overlappedHandle;
    ULONG ulBytesReturned;

    ZeroMemory(&overlapped, sizeof(overlapped));

    if (!pulBytesReturned)
    {
        pulBytesReturned = &ulBytesReturned;
    }

    auto clearOnFailure = wil::scope_exit([&](){ 
        *pulBytesReturned = 0;
    });

    overlappedHandle.reset(CreateEvent(NULL,FALSE,FALSE,NULL));
    RETURN_LAST_ERROR_IF(overlappedHandle.get() == nullptr);
    overlapped.hEvent = overlappedHandle.get();

    // Flag the event by setting the low-order bit so we
    // don't get completion port notifications.
    // Really! - see the description of the lpOverlapped parameter in
    // the docs for GetQueuedCompletionStatus
    overlapped.hEvent = (HANDLE)((DWORD_PTR)overlapped.hEvent | 0x1);

    BOOL fRes = DeviceIoControl(
        hHandle, 
        ulIoctl, 
        pvInBuffer, 
        cbInBuffer, 
        pvOutBuffer, 
        cbOutBuffer, 
        pulBytesReturned, 
        &overlapped);
    if (!fRes)
    {
        DWORD lastError = GetLastError();

        if (lastError == ERROR_IO_PENDING)
        {
            lastError = ERROR_SUCCESS;
            fRes = GetOverlappedResult(hHandle, &overlapped, pulBytesReturned, TRUE);
            if (!fRes)
            {
                lastError = GetLastError();
            }
        }

        RETURN_IF_FAILED(HRESULT_FROM_WIN32(lastError));
    }

    clearOnFailure.release();

    return S_OK;
}

_Use_decl_annotations_
HRESULT
PinPropertySimple(
    HANDLE Filter,
    ULONG   PinId,
    REFGUID GuidPropertySet,
    ULONG   Property,
    PVOID   Value,
    ULONG   ValueSize)
{
    KSP_PIN ksPProp;

    ksPProp.Property.Set = GuidPropertySet;
    ksPProp.Property.Id = Property;
    ksPProp.Property.Flags = KSPROPERTY_TYPE_GET;
    ksPProp.PinId = PinId;
    ksPProp.Reserved = 0;

    RETURN_IF_FAILED(SyncIoctl(
        Filter,
        IOCTL_KS_PROPERTY,
        &ksPProp,
        sizeof(KSP_PIN),
        Value,
        ValueSize,
        nullptr));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
FilterInstantiate(
    const WCHAR* FilterName,
    HANDLE* FilterHandle
)
{
    wil::unique_hfile localFilter(CreateFileW(  
                                    FilterName,
                                    GENERIC_READ | GENERIC_WRITE,
                                    0,
                                    NULL,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                                    NULL));

    if (!localFilter)
    {
        DWORD dwError = GetLastError();
        dwError = (dwError != ERROR_SUCCESS ? dwError : ERROR_GEN_FAILURE);
        RETURN_IF_FAILED(HRESULT_FROM_WIN32(dwError));
    }

    *FilterHandle = localFilter.release();
    
    return S_OK;
}

_Use_decl_annotations_
HRESULT
InstantiateMidiPin(
    HANDLE Filter,
    ULONG PinId,
    BOOL Looped,
    BOOL UMP,
    HANDLE* PinHandle
)
{
    HRESULT hr {S_OK};
    PKSPIN_CONNECT connect {nullptr};
    PKSDATAFORMAT dataFormat {nullptr};
    HANDLE pinLocal {NULL};

    struct {
        KSPIN_CONNECT Connect {0};
        KSDATAFORMAT  DataFormat {0};
    } PinConnectBlob;

    ZeroMemory(&PinConnectBlob, sizeof(PinConnectBlob));

    connect = &PinConnectBlob.Connect;
    dataFormat = &PinConnectBlob.DataFormat;

    // KSPIN_INTERFACE
    connect->Interface.Set =   KSINTERFACESETID_Standard;
    connect->Interface.Id  =   Looped? KSINTERFACE_STANDARD_LOOPED_STREAMING : KSINTERFACE_STANDARD_STREAMING;
    connect->Interface.Flags = 0;

    // KSPIN_MEDIUM
    connect->Medium.Set    =   KSMEDIUMSETID_Standard;
    connect->Medium.Id     =   KSMEDIUM_TYPE_ANYINSTANCE;
    connect->Medium.Flags  =   0;

    connect->PinId         =   PinId;
    connect->PinToHandle   =   NULL;

    // KSPRIORITY
    connect->Priority.PriorityClass = KSPRIORITY_NORMAL;
    connect->Priority.PrioritySubClass = KSPRIORITY_NORMAL;

    // Fill data format.
    dataFormat->FormatSize = sizeof(KSDATAFORMAT);
    dataFormat->MajorFormat = KSDATAFORMAT_TYPE_MUSIC;
    dataFormat->SubFormat = UMP?KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET:KSDATAFORMAT_SUBTYPE_MIDI;
    dataFormat->Specifier = KSDATAFORMAT_SPECIFIER_NONE;
   
    // create the MIDI pin
    hr = HRESULT_FROM_WIN32(KsCreatePin(
        Filter,
        &PinConnectBlob.Connect,
        GENERIC_WRITE | GENERIC_READ,
        &pinLocal));

    // ERROR_NO_MATCH expected when this pin does not support the requested message type or transport
    RETURN_IF_FAILED_WITH_EXPECTED(hr, HRESULT_FROM_WIN32(ERROR_NO_MATCH));

    // SUCCESS.
    *PinHandle = pinLocal;

    return S_OK;
}

