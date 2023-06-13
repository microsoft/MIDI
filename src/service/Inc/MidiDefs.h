// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#define HRESULT_FROM_RPCSTATUS(status) status == RPC_S_OK ? S_OK : MAKE_HRESULT(SEVERITY_ERROR, FACILITY_RPC, status)


