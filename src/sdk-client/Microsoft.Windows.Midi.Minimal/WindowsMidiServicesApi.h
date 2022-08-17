// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

// ====================================================================
// PRE-RELEASE VERSION. BREAKING CHANGES LIKELY. NOT FOR PRODUCTION USE.
// For more information, please see https://github.com/microsoft/midi
// ====================================================================

#pragma once

#ifdef WINDOWSMIDISERVICES_EXPORTS
#define WINDOWSMIDISERVICES_API __declspec(dllexport)
#else
#define WINDOWSMIDISERVICES_API __declspec(dllimport)
#endif

// This is just a convenience file so you don't need to include all of these
// in each file you have referencing the API

#include "WindowsMidiServicesUmp.h"
#include "WindowsMidiServicesMessages.h"
#include "WindowsMidiServicesUtility.h"
#include "WindowsMidiServicesEnumeration.h"
#include "WindowsMidiServicesSession.h"
#include "WindowsMidiServicesServiceControl.h"
