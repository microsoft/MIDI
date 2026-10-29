// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Sharing layouts and themes as packs, and saying who made them. Used by the library and the
// editor, so the dialogs are free functions rather than members of either window.

#include "ContentPack.h"
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

    // For a pack that arrived some other way than as a file: from a paired PC by "Sync to device".
    struct PackImportOptions
    {
        // The paired PC it came from, named in the dialog.
        std::wstring FromDevice{};

        // The person here already agreed to take this layout from that PC since Glass started, so
        // it replaces the copy here without asking again. Never for a copy changed here.
        bool AlreadyApproved{ false };

        // The copy here was changed since that PC last sent it. Says so, and always asks.
        bool ChangedHere{ false };

        // The layout this updates, for one made before layouts carried an id.
        std::wstring ExistingPathOverride{};

        // After the person agreed to add or replace it (not "keep both"), and it was installed.
        std::function<void()> Approved{};

        // No notice at the end: the caller says what happened its own way.
        bool Quiet{ false };
    };

    // The import dialog and install for a pack already opened and checked by
    // midiapp::OpenContentPack. ImportPackAsync ends here too, so a pack from the network gets
    // exactly what a pack file gets.
    winrt::Windows::Foundation::IAsyncAction ImportOpenedPackAsync(
        _In_ winrt::Microsoft::UI::Xaml::XamlRoot root,
        _In_ midiapp::OpenedContentPack opened,
        _In_ PackImportOptions options,
        _In_ std::function<void(ImportOutcome, std::wstring const&)> installed);

    // Everything a layout says about who made it, and whether a signature backs it. With
    // `changeInEditor`, a layout that can be changed offers to open it where that's done.
    winrt::Windows::Foundation::IAsyncAction ShowLayoutDetailsAsync(
        _In_ winrt::Microsoft::UI::Xaml::XamlRoot root,
        _In_ std::wstring layoutFilePath,
        _In_ std::function<void()> changeInEditor = nullptr);

    // The same for a theme. A theme file of the customer's own can be changed here, and the
    // result says whether it was.
    winrt::Windows::Foundation::IAsyncOperation<bool> ShowThemeDetailsAsync(
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
