// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

HRESULT 
SyncIoctl(
    _In_ HANDLE  hHandle,
    _In_ ULONG   ulIoctl,
    _In_ PVOID   pvInBuffer,
    _In_ ULONG   cbInBuffer,
    _In_ PVOID   pvOutBuffer,
    _In_ ULONG   cbOutBuffer,
    _In_opt_ PULONG  pulBytesReturned
);

HRESULT
PinPropertySimple(
    _In_ HANDLE Filter,
    _In_ ULONG   PinId,
    _In_ REFGUID guidPropertySet,
    _In_ ULONG   Property,
    _In_ PVOID   Value,
    _In_ ULONG   ValueSize);

HRESULT
FilterInstantiate(
    _In_z_ const WCHAR* FilterName,
    _In_ HANDLE* FilterHandle
);

HRESULT
InstantiateMidiPin(
    _In_ HANDLE Filter,
    _In_ ULONG PinId,
    _In_ BOOL Looped,
    _In_ BOOL UMP,
    _In_ HANDLE* PinHandle
);

