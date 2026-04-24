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

Module Name:

    pch.h - Common header for driver files.

Abstract:

   This file contains the common includes and defines for driver.

Environment:

    Kernel-mode Driver Framework

--*/

#pragma once

#define PAGED_CODE_SEG __declspec(code_seg("PAGE"))
#define INIT_CODE_SEG __declspec(code_seg("INIT"))
#define NONPAGED_CODE_SEG __declspec(code_seg("NOPAGE"))

#ifdef __cplusplus
extern "C" {
#endif

#pragma warning(disable:4200)  //
#pragma warning(disable:4201)  // nameless struct/union
#pragma warning(disable:4214)  // bit field types other than int


#include <initguid.h>
#include <ntifs.h>
#include <ntddk.h>
#include <windef.h>
#include <ks.h>
#include <mmsystem.h>
#include <ksmedia.h>
#include <ntstrsafe.h>
#include <ntintsafe.h>
#include <wdmguid.h>
#include <devguid.h>
#include <Devpkey.h>
//#include <midiksdef.h>

#pragma warning(default:4200)
#pragma warning(default:4201)
#pragma warning(default:4214)

#ifdef __cplusplus
}
#endif

#include <wil\resource.h>

#include "Driver.h"
#include "Device.h"
#include "Circuit.h"
#include "Common.h"
#include "Stream.h"
#include "StreamEngine.h"
#include "NewDelete.h"

