// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#define USBMIDI_POOLTAG 'UbMD'  

_Post_maybenull_
_Must_inspect_result_
PVOID
operator new(
    _In_ size_t size,
    _In_ POOL_FLAGS poolFlags,
    _In_ ULONG tag = USBMIDI_POOLTAG
    );

_Post_maybenull_
_Must_inspect_result_
PVOID
operator new[](
    _In_ size_t size,
    _In_ POOL_FLAGS poolFlags,
    _In_ ULONG tag = USBMIDI_POOLTAG
);

void __cdecl
operator delete(
    _Pre_maybenull_ __drv_freesMem(Mem) PVOID pVoid
    );

void __cdecl
operator delete[](
    _Pre_maybenull_ __drv_freesMem(Mem) PVOID pVoid
    );

void __cdecl
operator delete(
    _Pre_maybenull_ __drv_freesMem(Mem) PVOID pVoid,
    _In_ size_t size
    );

void __cdecl
operator delete[](
    _Pre_maybenull_ __drv_freesMem(Mem) PVOID pVoid,
    _In_ size_t size
    );
