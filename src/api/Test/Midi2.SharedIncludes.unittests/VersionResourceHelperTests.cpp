// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "VersionResourceHelperTests.h"
using namespace WindowsMidiServicesInternal;

void VersionResourceHelperTests::TestGetVersion()
{
    auto version = GetCurrentModuleVersion();

    VERIFY_ARE_EQUAL(L"1.0.15.0", version);
}

void VersionResourceHelperTests::TestGetCompanyName()
{
    auto name = GetCurrentModuleVersionCompanyName();

    VERIFY_ARE_EQUAL(L"Microsoft Corporation", name);
}


