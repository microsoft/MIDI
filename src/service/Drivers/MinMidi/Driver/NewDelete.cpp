// Copyright (c) Microsoft Corporation. All rights reserved.
#include <wdm.h>
#include "NewDelete.h"

#pragma code_seg()

_Use_decl_annotations_
PVOID
operator new(
    size_t iSize,
    POOL_FLAGS poolFlags,
    ULONG tag
)
{
    return ExAllocatePool2(poolFlags, iSize, tag);
}

_Use_decl_annotations_
PVOID
operator new[](
    size_t iSize,
    POOL_FLAGS poolFlags,
    ULONG tag
)
{
    return ExAllocatePool2(poolFlags, iSize, tag);
}

_Use_decl_annotations_
void __cdecl
operator delete(
    PVOID pVoid
)
{
    if (pVoid)
    {
        ExFreePoolWithTag(pVoid, MINMIDI_POOLTAG);
    }
}

_Use_decl_annotations_
void __cdecl
operator delete[](
    PVOID pVoid
)
{
    if (pVoid)
    {
        ExFreePoolWithTag(pVoid, MINMIDI_POOLTAG);
    }
}

_Use_decl_annotations_
void __cdecl
operator delete(
    PVOID pVoid,
    size_t
)
{
    if (pVoid)
    {
        ExFreePoolWithTag(pVoid, MINMIDI_POOLTAG);
    }
}

_Use_decl_annotations_
void __cdecl
operator delete[](
    PVOID pVoid,
    size_t
)
{
    if (pVoid)
    {
        ExFreePoolWithTag(pVoid, MINMIDI_POOLTAG);
    }
}

