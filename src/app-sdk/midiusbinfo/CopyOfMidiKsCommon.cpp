// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

// pulling this in as a copy because the original has additional baggage, and linking
// to the library itself causes a viral set of WinRT version mismatches that would
// mess up the main in-box project when compiled in-house. So, working with a copy.
// The MisiKsCommon header file from the service projects is still the one pointed to 
// from here to help with some very basic checking for changes.

#include "pch.h"

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

    auto clearOnFailure = wil::scope_exit([&]() {
        *pulBytesReturned = 0;
        });

    overlappedHandle.reset(CreateEvent(NULL, FALSE, FALSE, NULL));
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

    RETURN_IF_FAILED_WITH_EXPECTED(SyncIoctl(
        Filter,
        IOCTL_KS_PROPERTY,
        &ksPProp,
        sizeof(KSP_PIN),
        Value,
        ValueSize,
        nullptr),
        HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
PinPropertyAllocate(
    HANDLE Filter,
    ULONG   PinId,
    REFGUID GuidPropertySet,
    ULONG   Property,
    PVOID* Value,
    ULONG* ValueSize)
{
    KSP_PIN ksPProp;
    ULONG bytesRequired{ 0 };

    ksPProp.Property.Set = GuidPropertySet;
    ksPProp.Property.Id = Property;
    ksPProp.Property.Flags = KSPROPERTY_TYPE_GET;
    ksPProp.PinId = PinId;
    ksPProp.Reserved = 0;

    HRESULT hr = SyncIoctl(
        Filter,
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
            Filter,
            IOCTL_KS_PROPERTY,
            &ksPProp,
            sizeof(KSP_PIN),
            data.get(),
            bytesRequired,
            nullptr));

        *Value = (PVOID)data.release();
        if (ValueSize)
        {
            *ValueSize = bytesRequired;
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
    MidiTransport Transport,
    HANDLE* PinHandle
)
{
    HRESULT hr{ S_OK };
    PKSPIN_CONNECT connect{ nullptr };
    PKSDATAFORMAT dataFormat{ nullptr };
    HANDLE pinLocal{ NULL };

    struct {
        KSPIN_CONNECT Connect{ 0 };
        KSDATAFORMAT  DataFormat{ 0 };
    } PinConnectBlob;

    ZeroMemory(&PinConnectBlob, sizeof(PinConnectBlob));

    connect = &PinConnectBlob.Connect;
    dataFormat = &PinConnectBlob.DataFormat;

    BOOL looped = ((Transport == MidiTransport_CyclicByteStream) || (Transport == MidiTransport_CyclicUMP));
    BOOL ump = (Transport == MidiTransport_CyclicUMP);

    // KSPIN_INTERFACE
    connect->Interface.Set = KSINTERFACESETID_Standard;
    connect->Interface.Id = looped ? KSINTERFACE_STANDARD_LOOPED_STREAMING : KSINTERFACE_STANDARD_STREAMING;
    connect->Interface.Flags = 0;

    // KSPIN_MEDIUM
    connect->Medium.Set = KSMEDIUMSETID_Standard;
    connect->Medium.Id = KSMEDIUM_TYPE_ANYINSTANCE;
    connect->Medium.Flags = 0;

    connect->PinId = PinId;
    connect->PinToHandle = NULL;

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

_Use_decl_annotations_
BOOL
IsAutogeneratedPinName(
    const WCHAR* FilterName,
    ULONG PinId,
    const WCHAR* NameToCheck
)
{
    // we do a simple string comparison here. If there are more definitive
    // ways to check to see if the pin name was created and not read from iJack
    // then we can improve this shared function with that

    std::wstring generated(std::wstring(FilterName) + L" [" + std::to_wstring(PinId) + L"]");
    std::wstring nameToCheck(NameToCheck);

    // we check back to front since that's where the generated bits happen
    return std::equal(generated.rbegin(), generated.rend(), nameToCheck.rbegin());
}
