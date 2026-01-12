// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once


#ifndef MIDICHECKSERVICE_RETURN_CODES_H
#define MIDICHECKSERVICE_RETURN_CODES_H


// success return codes from this app
#define MIDISRV_CHECK_RETURN_VALUE_SUCCESS                      0   // service has been installed and enabled
#define MIDISRV_CHECK_RETURN_VALUE_SUCCESS_DEV_BUILD           -1   // service works but is a developer build, not the in-box build

// failure return codes
#define MIDISRV_CHECK_RETURN_VALUE_NOT_INSTALLED                1   // service has not been installed, because the COM interfaces are not present
#define MIDISRV_CHECK_RETURN_VALUE_NOT_ENABLED_OR_NOT_STARTED   2   // COM interface available, but service cannot be started. This may be due to Controlled Feature Rollout, or a system problem.

// indeterminate return codes
#define MIDISRV_CHECK_RETURN_VALUE_CHECK_SKIPPED                99  // An option was passed which skipped evaluation. For example, --help




#endif