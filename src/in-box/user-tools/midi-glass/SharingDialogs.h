// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Sharing layouts and themes as packs, and saying who made them. Used by the library and the
// editor, so the dialogs are free functions rather than members of either window.

#include "ContentProvenance.h"
#include "ThemeModel.h"

namespace midiglass::sharing
{
    // A layout pack, after asking what it should say about who made it and whether to sign it.
    // `apply` is for a window that has the layout open: it takes the change and saves the file,
    // so the file isn't written underneath it. Without it, the file is written here.
    winrt::Windows::Foundation::IAsyncAction ShareLayoutAsync(
        _In_ winrt::Microsoft::UI::Xaml::XamlRoot root,
        _In_ HWND owner,
        _In_ std::wstring layoutFilePath,
        _In_ std::function<bool(midiapp::ContentProvenance const&)> apply = nullptr);

    // A theme pack, the same way. Only a customer theme has a file to pack.
    winrt::Windows::Foundation::IAsyncAction ShareThemeAsync(
        _In_ winrt::Microsoft::UI::Xaml::XamlRoot root,
        _In_ HWND owner,
        _In_ std::wstring themeFilePath);

    enum class ImportOutcome
    {
        Nothing,
        Layout,
        Theme,
    };

    // A .midilayoutpack or .midithemepack: checks it, shows what it says and who signed it, and
    // installs it when the customer agrees. `installed` is called on the UI thread with the
    // layout or theme file it wrote.
    winrt::Windows::Foundation::IAsyncAction ImportPackAsync(
        _In_ winrt::Microsoft::UI::Xaml::XamlRoot root,
        _In_ std::wstring packPath,
        _In_ std::function<void(ImportOutcome, std::wstring const&)> installed);

    // Everything a layout or theme says about who made it, and whether a signature backs it.
    winrt::Windows::Foundation::IAsyncAction ShowLayoutDetailsAsync(
        _In_ winrt::Microsoft::UI::Xaml::XamlRoot root,
        _In_ std::wstring layoutFilePath);

    winrt::Windows::Foundation::IAsyncAction ShowThemeDetailsAsync(
        _In_ winrt::Microsoft::UI::Xaml::XamlRoot root,
        _In_ glass::Theme theme);

    // The name, group, website and license put on everything made on this PC.
    winrt::Windows::Foundation::IAsyncAction EditAuthorProfileAsync(
        _In_ winrt::Microsoft::UI::Xaml::XamlRoot root);

    // "by Name", with "(unverified)" unless a trusted signature backs it. Empty when nobody is named.
    std::wstring DescribeMaker(
        _In_ std::optional<midiapp::ContentProvenance> const& provenance,
        _In_ std::wstring const& signerName);

    // Who signed the file, while it is still exactly what was signed. Empty otherwise.
    std::wstring SignerOf(_In_ std::wstring const& filePath);

    // The file picker filters for packs and old layout packages, for "Import".
    std::wstring PickImportPath(_In_ HWND owner);
}
