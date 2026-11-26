// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Packs come from strangers. Every rule the reader enforces is here, with a pack built to break
// it: a changed byte, a file the list doesn't name, two names that differ only in case, a name
// that climbs out of the folder, and a signature that doesn't match.
class ContentPackTests
{
    BEGIN_TEST_CLASS(ContentPackTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Type", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD_SETUP(Setup);
    TEST_METHOD_CLEANUP(Cleanup);

    // ---- the zip underneath ----

    TEST_METHOD(AStoredZipRoundTrips);
    TEST_METHOD(AChangedByteIsDamage);
    TEST_METHOD(ADirectoryThatDisagreesIsRefused);
    TEST_METHOD(ACompressedEntryIsNamedAsSuch);
    TEST_METHOD(TooManyEntriesAreRefused);
    TEST_METHOD(SomethingElseIsNotAZip);

    // ---- the pack ----

    TEST_METHOD(APackRoundTrips);
    TEST_METHOD(PackPathsAreBareAndSafe);
    TEST_METHOD(AChangedFileIsCaught);
    TEST_METHOD(AFileTheListDoesNotNameIsRefused);
    TEST_METHOD(AFileTheListNamesMustBeThere);
    TEST_METHOD(NamesThatDifferOnlyInCaseAreRefused);
    TEST_METHOD(ANameThatClimbsOutIsRefused);
    TEST_METHOD(TheKindIsChecked);
    TEST_METHOD(ANewerFormatIsNamedAsSuch);

    // ---- signatures ----

    TEST_METHOD(ASelfSignedPackIsNotTrusted);
    TEST_METHOD(ASignatureFromATrustedRootIsTrusted);
    TEST_METHOD(ASignatureForAnotherPackIsBroken);
    TEST_METHOD(ADamagedSignatureIsBroken);
    TEST_METHOD(AddingASignatureKeepsTheFiles);

    // ---- layouts and themes ----

    TEST_METHOD(ALayoutPackCarriesItsPictures);
    TEST_METHOD(AnInstallNeverWritesOverAnotherPicture);
    TEST_METHOD(ASignedItemIsOnlySignedWhileUnchanged);
};
