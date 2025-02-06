/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Pch.h

Abstract:

    Precompiled header.

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
#include "ntifs.h"
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

