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
#include "MidiDefs.h"
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

    // if a bytes returned pointer was not provided,
    // supply our own.
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

        // ERROR_MORE_DATA is an expected error code, and the
        // output buffer size needs to be retained for this error.
        if (lastError == ERROR_MORE_DATA)
        {
            clearOnFailure.release();
            return HRESULT_FROM_WIN32(ERROR_MORE_DATA);
        }

        RETURN_IF_FAILED_WITH_EXPECTED(HRESULT_FROM_WIN32(lastError),
                                        HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));
    }

    clearOnFailure.release();

    return S_OK;
}

_Use_decl_annotations_
HRESULT
PinPropertySimple(
    HANDLE filter,
    ULONG   pinId,
    REFGUID guidPropertySet,
    ULONG   property,
    PVOID   value,
    ULONG   valueSize)
{
    KSP_PIN ksPProp;

    ksPProp.Property.Set = guidPropertySet;
    ksPProp.Property.Id = property;
    ksPProp.Property.Flags = KSPROPERTY_TYPE_GET;
    ksPProp.PinId = pinId;
    ksPProp.Reserved = 0;

    RETURN_IF_FAILED_WITH_EXPECTED(SyncIoctl(
        filter,
        IOCTL_KS_PROPERTY,
        &ksPProp,
        sizeof(KSP_PIN),
        value,
        valueSize,
        nullptr),
        HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
PinPropertyAllocate(
    HANDLE filter,
    ULONG   pinId,
    REFGUID guidPropertySet,
    ULONG   property,
    PVOID   *value,
    ULONG   *valueSize)
{
    KSP_PIN ksPProp;
    ULONG bytesRequired {0};

    ksPProp.Property.Set = guidPropertySet;
    ksPProp.Property.Id = property;
    ksPProp.Property.Flags = KSPROPERTY_TYPE_GET;
    ksPProp.PinId = pinId;
    ksPProp.Reserved = 0;

    HRESULT hr = SyncIoctl(
        filter,
        IOCTL_KS_PROPERTY,
        &ksPProp,
        sizeof(KSP_PIN),
        nullptr,
        0,
        &bytesRequired);

    if (hr == HRESULT_FROM_WIN32(ERROR_MORE_DATA) && bytesRequired > 0)
    {
        std::unique_ptr<BYTE> data;

        data.reset(new (std::nothrow) BYTE[bytesRequired]);
        RETURN_IF_NULL_ALLOC(data);

        RETURN_IF_FAILED(SyncIoctl(
            filter,
            IOCTL_KS_PROPERTY,
            &ksPProp,
            sizeof(KSP_PIN),
            data.get(),
            bytesRequired,
            nullptr));

        *value = (PVOID) data.release();
        if (valueSize)
        {
            *valueSize = bytesRequired;
        }
        return S_OK;
    }

    RETURN_IF_FAILED_WITH_EXPECTED(hr, HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));

    // The call to retrieve the buffer size returned S_OK, which is unexpected
    return E_UNEXPECTED;
}

_Use_decl_annotations_
HRESULT
FilterInstantiate(
    const WCHAR* filterName,
    HANDLE* filterHandle
)
{
    wil::unique_hfile localFilter(CreateFileW(  
                                    filterName,
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

    *filterHandle = localFilter.release();
    
    return S_OK;
}

_Use_decl_annotations_
HRESULT
InstantiateMidiPin(
    HANDLE filter,
    ULONG pinId,
    MidiTransport transport,
    HANDLE* pinHandle
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

    BOOL looped = ((transport == MidiTransport_CyclicByteStream) || (transport == MidiTransport_CyclicUMP));
    BOOL ump = (transport == MidiTransport_CyclicUMP);

    // KSPIN_INTERFACE
    connect->Interface.Set =   KSINTERFACESETID_Standard;
    connect->Interface.Id  =   looped ? KSINTERFACE_STANDARD_LOOPED_STREAMING : KSINTERFACE_STANDARD_STREAMING;
    connect->Interface.Flags = 0;

    // KSPIN_MEDIUM
    connect->Medium.Set    =   KSMEDIUMSETID_Standard;
    connect->Medium.Id     =   KSMEDIUM_TYPE_ANYINSTANCE;
    connect->Medium.Flags  =   0;

    connect->PinId         =   pinId;
    connect->PinToHandle   =   NULL;

    // KSPRIORITY
    connect->Priority.PriorityClass = KSPRIORITY_NORMAL;
    connect->Priority.PrioritySubClass = KSPRIORITY_NORMAL;

    // Fill data format.
    dataFormat->FormatSize = sizeof(KSDATAFORMAT);
    dataFormat->MajorFormat = KSDATAFORMAT_TYPE_MUSIC;
    dataFormat->SubFormat = ump ? KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET : KSDATAFORMAT_SUBTYPE_MIDI;
    dataFormat->Specifier = KSDATAFORMAT_SPECIFIER_NONE;
   
    // create the MIDI pin
    hr = HRESULT_FROM_WIN32(KsCreatePin(
        filter,
        &PinConnectBlob.Connect,
        GENERIC_WRITE | GENERIC_READ,
        &pinLocal));

    // ERROR_NO_MATCH expected when this pin does not support the requested message type or transport
    RETURN_IF_FAILED_WITH_EXPECTED(hr, HRESULT_FROM_WIN32(ERROR_NO_MATCH));

    // SUCCESS.
    *pinHandle = pinLocal;

    return S_OK;
}

BOOL
IsAutogeneratedPinName(
    const WCHAR* filterName,
    ULONG pinId,
    const WCHAR* name
)
{
    // we do a simple string comparison here. If there are more definitive
    // ways to check to see if the pin name was created and not read from iJack
    // then we can improve this shared function with that

    std::wstring generated(std::wstring(filterName) + L" [" + std::to_wstring(pinId) + L"]");
    std::wstring nameToCheck(name);

    // we check back to front since that's where the generated bits happen
    return std::equal(generated.rbegin(), generated.rend(), nameToCheck.rbegin());
}




