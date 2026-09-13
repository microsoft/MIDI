// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class ToastSender
{
public:
    // Shows or, when a toast with the same tag is already up, replaces it. Replacing is what
    // keeps a second device from stacking a second banner.
    HRESULT ShowOrReplace(
        _In_ std::wstring const& tag,
        _In_ std::wstring const& title,
        _In_ std::wstring const& body,
        _In_ std::wstring const& attribution,
        _In_ std::wstring const& buttonText,
        _In_ std::wstring const& buttonProtocolUri) noexcept;

    // Withdraws a toast the customer no longer needs to see, for instance because the remote
    // gave up while the banner was still sitting in the notification center.
    void Remove(_In_ std::wstring const& tag) noexcept;

    // Text which came off the network is attacker controlled and toast payloads are XML, so it
    // is escaped and capped before it goes anywhere near the document.
    static std::wstring SanitizeForToast(_In_ std::wstring const& untrusted) noexcept;

private:
    static constexpr wchar_t ToastGroup[]{ L"midi" };

    // Long enough to recognize a device, short enough that a padded name cannot push the rest of
    // the message off the banner.
    static constexpr size_t MaximumUntrustedTextLength{ 64 };
};
