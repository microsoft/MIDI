// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, like the rest of the document layer. The layout reader
// and writer call these for the "remoteControl" object, which is kept in its own file so the
// rules can be read and tested on their own.

#include <sal.h>
#include <string_view>

#include "LayoutModel.h"
#include "JsonText.h"

namespace glass
{
    // The layout's MIDI remote control object. Never throws and never trusts the file: rules are
    // capped, names are bounded, and anything malformed is dropped rather than believed.
    // namesUnknownAction is set when a rule names an action this build does not know, which
    // means a newer build wrote the file.
    void ReadRemoteControl(
        _In_ winrt::Windows::Data::Json::JsonObject const& object,
        _Out_ RemoteControl& remote,
        _Out_ bool& namesUnknownAction) noexcept;

    // Whether there is anything to write. A layout with no rules writes no object, so older files
    // come back byte for byte.
    bool HasRemoteControl(_In_ RemoteControl const& remote) noexcept;

    // Writes the object under this key, if there is anything to write.
    void WriteRemoteControl(
        _Inout_ JsonTextWriter& writer,
        _In_ std::wstring_view key,
        _In_ RemoteControl const& remote) noexcept;
}
