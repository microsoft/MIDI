// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiapp
{
    // The pictures customers attach to their endpoints live in one shared folder so that every
    // MIDI tool shows the same artwork for the same endpoint, and so the configuration file only
    // ever has to name a file rather than carry a path.
    //
    // This mirrors the MIDI Settings app's MidiEndpointImageService: same folder, same "ep-"
    // prefix, same "reuse an identical file rather than making another copy" rule, so a picture
    // chosen in one tool is the same file another tool would have chosen.
    //
    // Nothing here throws. A failure returns an empty string or false and leaves the folder alone.
    class EndpointImageAssets
    {
    public:
        // %allusersprofile%\Microsoft\MIDI\Assets\Endpoints, expanded. Empty if it cannot be
        // resolved.
        static std::wstring FolderPath() noexcept;

        // Full path of a stored asset. The name is cleaned first, so a caller cannot be talked
        // into reading outside the folder by a hand-edited configuration file. Empty if the name
        // is not usable.
        static std::wstring FullPathForFileName(_In_ std::wstring const& fileName) noexcept;

        static bool Exists(_In_ std::wstring const& fileName) noexcept;

        // Copies the customer's chosen file into the shared folder and hands back the bare file
        // name to store. An identical file already there is reused rather than duplicated, and a
        // different file with the same name gets a "(1)", "(2)" suffix. Empty on failure, with
        // the reason in errorMessage.
        static std::wstring CopyIntoFolder(
            _In_ std::wstring const& sourcePath,
            _Out_ std::wstring& errorMessage) noexcept;

        // The extensions the pickers offer. SVG needs a different decoder from the rest, which
        // is why callers need to be able to tell them apart.
        static bool IsScalableVector(_In_ std::wstring const& fileName) noexcept;

        // Asks the customer for a picture and hands back the full path they chose, or an empty
        // string if they canceled.
        //
        // This is the Win32 common item dialog rather than Windows.Storage.Pickers on purpose:
        // the WinRT picker's completion never resumes when it is raised over an open
        // ContentDialog, which is exactly where these tools ask for a picture. The Win32 dialog
        // pumps its own modal loop and returns a result directly, so there is no continuation to
        // lose. It blocks until dismissed, so call it on the UI thread and do the copying after.
        static std::wstring ShowPicker(_In_ HWND const owner) noexcept;
    };
}
