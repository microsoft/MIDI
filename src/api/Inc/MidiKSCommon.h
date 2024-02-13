// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


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
PinPropertyAllocate(
    _In_ HANDLE Filter,
    _In_ ULONG   PinId,
    _In_ REFGUID guidPropertySet,
    _In_ ULONG   Property,
    _Out_ PVOID   *Value,
    _Out_opt_ ULONG   *ValueSize);

HRESULT
FilterInstantiate(
    _In_z_ const WCHAR* FilterName,
    _In_ HANDLE* FilterHandle
);

HRESULT
InstantiateMidiPin(
    _In_ HANDLE Filter,
    _In_ ULONG PinId,
    _In_ MidiTransport Transport,
    _In_ HANDLE* PinHandle
);

