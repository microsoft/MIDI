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
