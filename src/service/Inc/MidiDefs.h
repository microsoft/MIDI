// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

using unique_mmcss_handle = wil::unique_any<HANDLE, decltype(&::AvRevertMmThreadCharacteristics), AvRevertMmThreadCharacteristics>;
using unique_viewoffile = wil::unique_any<LPVOID, decltype(&::UnmapViewOfFile), UnmapViewOfFile>;

#define HRESULT_FROM_RPCSTATUS(status) status == RPC_S_OK ? S_OK : MAKE_HRESULT(SEVERITY_ERROR, FACILITY_RPC, status)

#define SAFE_CLOSEHANDLE(h) if (h) { CloseHandle(h); h = NULL; }

