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
// Copyright (c) Microsoft Corporation. All rights reserved.
#include "Pch.h"

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
        ExFreePoolWithTag(pVoid, USBMIDI_POOLTAG);
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
        ExFreePoolWithTag(pVoid, USBMIDI_POOLTAG);
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
        ExFreePoolWithTag(pVoid, USBMIDI_POOLTAG);
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
        ExFreePoolWithTag(pVoid, USBMIDI_POOLTAG);
    }
}

