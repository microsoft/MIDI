// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// this file is shared with the client SDK IDL file and so must contain
// defines that are valid in C++ and MIDL

#define BASIC_LOOPBACK_ERROR_CODE_UNKNOWN_ERROR             0x00000000
#define BASIC_LOOPBACK_ERROR_CODE_UNRECOGNIZED_COMMAND      0x00000001
#define BASIC_LOOPBACK_ERROR_CODE_INVALID_JSON              0x00000011

#define BASIC_LOOPBACK_ERROR_CODE_ENDPOINT_CREATION_FAILED  0x00000021

#define BASIC_LOOPBACK_ERROR_CODE_MISSING_ASSOCIATION_ID    0x00000031

#define BASIC_LOOPBACK_ERROR_CODE_DUPLICATE_UNIQUE_ID       0x00000141
#define BASIC_LOOPBACK_ERROR_CODE_MISSING_UNIQUE_ID         0x00000142
#define BASIC_LOOPBACK_ERROR_CODE_MISSING_ENDPOINT_NAME     0x00000143
#define BASIC_LOOPBACK_ERROR_CODE_DUPLICATE_ENDPOINT_NAME   0x00000144


// update / remove
#define BASIC_LOOPBACK_ERROR_CODE_ENDPOINT_NOT_FOUND        0x00001065

