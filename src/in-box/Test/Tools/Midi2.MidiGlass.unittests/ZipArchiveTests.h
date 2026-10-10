// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The zip and deflate code every tool shares. Windows' own MSZIP codec, which is deflate in 32 KB
// pieces, checks the deflate code from outside in both directions.
class ZipArchiveTests
{
    BEGIN_TEST_CLASS(ZipArchiveTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Type", L"Unit")
    END_TEST_CLASS()

    // ---- deflate ----

    TEST_METHOD(DeflateRoundTrips);
    TEST_METHOD(WindowsReadsWhatWeCompress);
    TEST_METHOD(WeReadWhatWindowsCompresses);
    TEST_METHOD(DamagedStreamsAreRefused);

    // ---- zips ----

    TEST_METHOD(TheStoredWriterMakesTheSameBytesAsBefore);
    TEST_METHOD(ADeflatedZipRoundTrips);
    TEST_METHOD(ZipsFromOtherToolsOpen);
    TEST_METHOD(OldCodePageNamesAreRead);
    TEST_METHOD(HostileZipsAreRefused);
    TEST_METHOD(AZipFileCanHaveAnyName);
    TEST_METHOD(AnUnfinishedZipLeavesNothingBehind);
    TEST_METHOD(NamesThatCouldClimbOutAreRefused);
};
