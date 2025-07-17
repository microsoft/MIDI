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
/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Driver.h

Abstract:

    Contains structure definitions and function prototypes private to
    the driver.

Environment:

    Kernel mode

--*/

#ifndef _DRIVER_H_
#define _DRIVER_H_

/* make prototypes usable from C++ */
#ifdef __cplusplus
extern "C" {
#endif

#include <ntifs.h>
#include <ntddk.h>
#include <wdf.h>
#include <usb.h>
#include <usbdlib.h>
#include <wdfusb.h>

#include <acx.h>

#include "Trace.h"

// Simple MIN MIDI 2 driver
#define DRIVER_TAG (ULONG) '2MiM'

// Number of millisecs per sec. 
#define MS_PER_SEC 1000

// Number of hundred nanosecs per sec. 
#define HNS_PER_SEC 10000000

// Number of msec for idle timeout.
#define IDLE_POWER_TIMEOUT 5000

// Compatible ID for render/capture
#define ACX_STANDALONE_TEST_COMPATIBLE_ID   L"{3251c107-2a7c-4b93-bb11-2261e304948e}"

// Container ID for render/capture
#define ACX_STANDALONE_TEST_CONTAINER_ID    L"{00000000-0000-0000-ffff-ffffffffffff}"

#undef MIN
#undef MAX
#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#ifndef BOOL
typedef int BOOL;
#endif

#ifndef SIZEOF_ARRAY
#define SIZEOF_ARRAY(ar)        (sizeof(ar)/sizeof((ar)[0]))
#endif // !defined(SIZEOF_ARRAY)

//
// Define driver context.
//
typedef struct _DRIVER_CONTEXT {
    BOOLEAN         Dummy;
} DRIVER_CONTEXT, *PDRIVER_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DRIVER_CONTEXT, GetDriverContext)

//
// Driver prototypes.
//
DRIVER_INITIALIZE       DriverEntry;
DRIVER_UNLOAD           DriverUnload;

#ifdef __cplusplus
}
#endif

//
// Used to store the registry settings path for the driver.
//
extern UNICODE_STRING g_RegistryPath;

_Must_inspect_result_
__drv_maxIRQL(PASSIVE_LEVEL)
PAGED_CODE_SEG
NTSTATUS
CopyRegistrySettingsPath(
    _In_ PUNICODE_STRING RegistryPath
    );

#endif // _DRIVER_H_
