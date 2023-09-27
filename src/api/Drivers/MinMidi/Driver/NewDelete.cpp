// Copyright (c) Microsoft Corporation. All rights reserved.
#include <wdm.h>
#include "NewDelete.h"

#pragma code_seg()

_Post_maybenull_
_Must_inspect_result_
PVOID
operator new(
    _In_ size_t iSize,
    _In_ POOL_FLAGS poolFlags,
    _In_ ULONG tag
)
{
    return ExAllocatePool2(poolFlags, iSize, tag);
}

_Post_maybenull_
_Must_inspect_result_
PVOID
operator new[](
    _In_ size_t iSize,
    _In_ POOL_FLAGS poolFlags,
    _In_ ULONG tag
)
{
    return ExAllocatePool2(poolFlags, iSize, tag);
}

void __cdecl
operator delete(
    _Pre_maybenull_ __drv_freesMem(Mem) PVOID pVoid
)
{
    if (pVoid)
    {
        ExFreePoolWithTag(pVoid, MINMIDI_POOLTAG);
    }
}

void __cdecl
operator delete[](
    _Pre_maybenull_ __drv_freesMem(Mem) PVOID pVoid
)
{
    if (pVoid)
    {
        ExFreePoolWithTag(pVoid, MINMIDI_POOLTAG);
    }
}

void __cdecl
operator delete(
    _Pre_maybenull_ __drv_freesMem(Mem) PVOID pVoid,
    _In_ size_t
)
{
    if (pVoid)
    {
        ExFreePoolWithTag(pVoid, MINMIDI_POOLTAG);
    }
}

void __cdecl
operator delete[](
    _Pre_maybenull_ __drv_freesMem(Mem) PVOID pVoid,
    _In_ size_t
)
{
    if (pVoid)
    {
        ExFreePoolWithTag(pVoid, MINMIDI_POOLTAG);
    }
}

