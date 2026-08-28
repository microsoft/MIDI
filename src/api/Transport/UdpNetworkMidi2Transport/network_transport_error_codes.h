// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// this file is shared with the client SDK IDL file and so must contain
// defines that are valid in C++ and MIDL. Keep it free of anything else.

#define NETWORK_ERROR_CODE_UNKNOWN_ERROR                            0x00000000
#define NETWORK_ERROR_CODE_UNRECOGNIZED_COMMAND                     0x00000001
#define NETWORK_ERROR_CODE_MISSING_COMMAND                          0x00000002
#define NETWORK_ERROR_CODE_INVALID_JSON                             0x00000011

// host lifecycle
#define NETWORK_ERROR_CODE_HOST_CREATION_FAILED                     0x00000021
#define NETWORK_ERROR_CODE_HOST_REMOVAL_FAILED                      0x00000022
#define NETWORK_ERROR_CODE_UNABLE_TO_START_HOST                     0x00000023
#define NETWORK_ERROR_CODE_UNABLE_TO_STOP_HOST                      0x00000024

// entry identifiers
#define NETWORK_ERROR_CODE_MISSING_ENTRY_IDENTIFIER                 0x00000031
#define NETWORK_ERROR_CODE_INVALID_ENTRY_IDENTIFIER                 0x00000032

// host definition
#define NETWORK_ERROR_CODE_MISSING_ENDPOINT_NAME                    0x00000041
#define NETWORK_ERROR_CODE_MISSING_PRODUCT_INSTANCE_ID              0x00000042
#define NETWORK_ERROR_CODE_SERVICE_INSTANCE_NAME_IN_USE             0x00000043
#define NETWORK_ERROR_CODE_INVALID_NETWORK_PROTOCOL                 0x00000044
#define NETWORK_ERROR_CODE_ENDPOINT_NAME_TOO_LONG                   0x00000045
#define NETWORK_ERROR_CODE_PRODUCT_INSTANCE_ID_TOO_LONG             0x00000046
#define NETWORK_ERROR_CODE_INVALID_PRODUCT_INSTANCE_ID              0x00000047
#define NETWORK_ERROR_CODE_INVALID_HOST_PORT                        0x00000048
#define NETWORK_ERROR_CODE_HOST_PORT_IN_USE                         0x00000049

// authentication
#define NETWORK_ERROR_CODE_MISSING_CREDENTIAL_IDENTIFIER            0x00000051
#define NETWORK_ERROR_CODE_INVALID_CREDENTIAL_IDENTIFIER            0x00000052
#define NETWORK_ERROR_CODE_AUTHENTICATION_NOT_IMPLEMENTED           0x00000053

// client connection
#define NETWORK_ERROR_CODE_MISSING_REMOTE_ADDRESS                   0x00000061
#define NETWORK_ERROR_CODE_MISSING_REMOTE_PORT                      0x00000062
#define NETWORK_ERROR_CODE_INVALID_REMOTE_PORT                      0x00000063
#define NETWORK_ERROR_CODE_MISSING_MATCH_ENTRY                      0x00000064
#define NETWORK_ERROR_CODE_MISSING_MATCH_ID                         0x00000065

// invitations and remote client approval
#define NETWORK_ERROR_CODE_NO_REPLY_TO_INVITATION                   0x00000071
#define NETWORK_ERROR_CODE_INVITATION_NOT_APPROVED                  0x00000072
#define NETWORK_ERROR_CODE_MISSING_REMOTE_CLIENT_IDENTITY           0x00000073
#define NETWORK_ERROR_CODE_PENDING_REMOTE_CLIENT_NOT_FOUND          0x00000074

// lookup failures
#define NETWORK_ERROR_CODE_HOST_NOT_FOUND                           0x00001065
#define NETWORK_ERROR_CODE_CLIENT_NOT_FOUND                         0x00001066
#define NETWORK_ERROR_CODE_REMOTE_CLIENT_NOT_FOUND                  0x00001067

// raised by the client SDK rather than the service
#define NETWORK_ERROR_CODE_CLIENT_API_INVALID_ARGUMENT              0x11000055
#define NETWORK_ERROR_CODE_CLIENT_API_EXCEPTION                     0x11002011
#define NETWORK_ERROR_CODE_CLIENT_API_TIMEOUT                       0x110005B4
