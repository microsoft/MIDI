// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include <sal.h>
#include <cstdint>
#include <string>
#include <vector>

#include "LayoutModel.h"

namespace glass
{
    // Putting a layout and everything it needs into one file, and taking it back out again.
    //
    // A layout is not one file. It names pictures and videos that live beside it, and a copy
    // that arrives without them is a surface full of empty rectangles. So a backup and a package
    // are the same thing: the layout plus every file it points at.
    //
    // The container is an ordinary zip, so somebody can look inside it with Explorer and see
    // what they are about to hand to a friend. Entries are STORED rather than deflated: a
    // layout is a few kilobytes of text and the pictures beside it are already compressed, so
    // there is nothing to win, and a decompressor fed a stranger's file is a liability this app
    // does not need. A package that has been re-compressed by another tool is refused by name
    // rather than failing halfway through.

    struct PackageResult
    {
        bool Succeeded{ false };

        // Where it ended up, for a package or a restore. Empty on failure.
        std::wstring Path{};

        // How many files traveled, including the layout itself.
        uint32_t FileCount{ 0 };

        // A resource key naming what went wrong, or empty. Not text: this layer never looks at
        // a resource file.
        std::wstring FailureKey{};
    };

    // The folder backups go in, made if it is not there. Empty when it cannot be made.
    std::wstring BackupsFolderPath() noexcept;

    // The next free backup for this layout, as a full path. Numbered rather than stamped with
    // the time, because "3" is something a person can say out loud and a timestamp is not.
    std::wstring NextBackupPath(_In_ std::wstring const& layoutFilePath) noexcept;

    // One backup that already exists.
    struct BackupEntry
    {
        std::wstring Path{};

        // The number in the file name.
        int32_t Number{ 0 };

        // When it was written, as a FILETIME.
        int64_t WrittenAt{ 0 };

        uint64_t Bytes{ 0 };
    };

    // Every backup of this layout, newest number first.
    std::vector<BackupEntry> ListBackups(_In_ std::wstring const& layoutFilePath) noexcept;

    // The layout plus every picture it names, into one zip. Used by both "back up now" and
    // "package for another PC" - they differ only in where the file lands.
    PackageResult WriteLayoutPackage(
        _In_ std::wstring const& layoutFilePath,
        _In_ std::wstring const& packagePath) noexcept;

    // Unpacks into a folder. The layout inside keeps its own name unless that name is taken,
    // in which case a number is added rather than writing over what is already there.
    //
    // Nothing is written outside `targetFolder`: an entry whose name is a path, or which tries
    // to climb out with "..", is dropped. A package is a file from a stranger.
    PackageResult ReadLayoutPackage(
        _In_ std::wstring const& packagePath,
        _In_ std::wstring const& targetFolder) noexcept;

    // Puts a backup back, over the layout it came from. The layout as it stands is backed up
    // first, so restoring the wrong one is not the end of the story.
    PackageResult RestoreLayoutFromBackup(
        _In_ std::wstring const& backupPath,
        _In_ std::wstring const& layoutFilePath) noexcept;

    // The extension a package carries. A plain .zip, because that is what it is.
    constexpr wchar_t LayoutPackageExtension[] = L".zip";

    // Nothing sane is bigger than this, and a package from a stranger must not be able to ask
    // this app to read a terabyte.
    constexpr uint64_t MaximumPackageBytes = 512ull * 1024 * 1024;
    constexpr uint32_t MaximumPackageEntries = 256;
}
