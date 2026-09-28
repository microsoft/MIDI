// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include <sal.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "LayoutSerializer.h"

namespace glass
{
    // One .midilayout per layout, in the customer's Documents folder, exactly the shape MIDI
    // Patchbay uses for patches. This is the only part of the document layer that touches a disk.
    //
    // The first builds wrote ".midilayout.json". Explorer sees that as a JSON file and never
    // offers this app for it, so those are still read, and renamed when the app starts.
    constexpr wchar_t LayoutFolderName[] = L"MIDI Layouts";
    constexpr wchar_t LayoutFileExtension[] = L".midilayout";
    constexpr wchar_t LegacyLayoutFileExtension[] = L".midilayout.json";

    // Whether a file name ends in this extension, ignoring case.
    bool HasFileExtension(_In_ std::wstring_view fileName, _In_ std::wstring_view extension) noexcept;

    // A layout file under either name.
    bool IsLayoutFileName(_In_ std::wstring_view fileName) noexcept;

    // The file name without its layout extension, old or new. Anything else comes back whole.
    std::wstring LayoutNameFromFileName(_In_ std::wstring const& fileName) noexcept;

    // Renames every file in the folder ending in the old extension to the same name with the new
    // one. A file whose new name is already taken is left as it is. Returns the old and new path
    // of each file renamed, so anything that remembered the old path can follow it.
    std::vector<std::pair<std::wstring, std::wstring>> RenameLegacyFiles(
        _In_ std::wstring const& folder,
        _In_ std::wstring_view legacyExtension,
        _In_ std::wstring_view extension) noexcept;

    // Created if it is not there. Empty when it cannot be.
    std::wstring LayoutsFolder() noexcept;

    // Reads and parses. A layout from outside the layouts folder is marked imported, which is
    // what makes the app ask before it sends system exclusive to a device on that layout's
    // behalf: a layout file is data, but a sequence step can hold arbitrary SysEx, and arbitrary
    // SysEx sent to the wrong device can do real damage to it.
    ReadResult ReadLayoutFile(_In_ std::wstring const& filePath) noexcept;

    // Writes the canonical form. Returns false without touching the file on any failure.
    bool WriteLayoutFile(_In_ LayoutDocument const& document, _In_ std::wstring const& filePath) noexcept;

    // Every layout in the layouts folder, by full path, sorted.
    std::vector<std::wstring> ListLayoutFiles() noexcept;

    // True when this path is inside the layouts folder.
    bool IsInLayoutsFolder(_In_ std::wstring const& filePath) noexcept;

    // A path in that folder for a layout with this name, with a number added if the name is
    // taken. Anything Windows will not accept in a file name is replaced rather than refused,
    // because a layout name is prose and nobody should have to guess which character was the
    // problem.
    std::wstring MakeUnusedLayoutPath(
        _In_ std::wstring const& folder,
        _In_ std::wstring const& layoutName) noexcept;

    // Full path of a layout's background picture, or empty when there is none, the name is not
    // a plain file name, or the file is not beside the layout. Only the layout's own folder is
    // ever read from, so a name that tries to climb out of it resolves to nothing.
    std::wstring BackgroundImagePath(_In_ LayoutDocument const& document) noexcept;

    // Copies a chosen picture next to the layout and hands back the bare file name to store.
    // Empty on any failure. A picture that is already beside the layout is used where it is.
    std::wstring CopyBackgroundImageBeside(
        _In_ std::wstring const& sourcePath,
        _In_ std::wstring const& layoutFilePath) noexcept;

    // True when this picture is not already beside that layout, so the customer can be told it
    // is about to be copied before it happens.
    bool BackgroundImageNeedsCopying(
        _In_ std::wstring const& sourcePath,
        _In_ std::wstring const& layoutFilePath) noexcept;

    // The same resolution as BackgroundImagePath, for a picture named by one control rather
    // than by the document. Empty when the name is not a plain file name or the file is not
    // beside the layout.
    std::wstring ControlPicturePath(
        _In_ std::wstring const& layoutFilePath,
        _In_ std::wstring const& fileName) noexcept;

    // The picture and video file types a layout may point at. Anything else is refused at the
    // picker, so a layout from a stranger cannot name a file type this app was not expecting.
    bool IsSupportedPictureFileName(_In_ std::wstring const& fileName) noexcept;
    bool IsVideoFileName(_In_ std::wstring const& fileName) noexcept;
}
