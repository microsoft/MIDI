// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include <ntifs.h>
#include <wdm.h>
#include <windef.h>
#include <ks.h>
#include <ksmedia.h>
#include <wil\resource.h>

#include "NewDelete.h"

#include "MidiKSDef.h"

#define MM_MAPPING_ADDRESS_DIVISIBLE 0x1

#define PAGED_CODE_SEG __declspec(code_seg("PAGE"))
#define INIT_CODE_SEG __declspec(code_seg("INIT"))

#define NT_RETURN_IF_NTSTATUS_FAILED(status) do {\
        if(!NT_SUCCESS(status)) { \
            return status; \
        } \
    } while(0)


#define NT_RETURN_NTSTATUS_IF(status, expression) do {\
    if(expression) { \
        return status; \
    } \
} while(0)

