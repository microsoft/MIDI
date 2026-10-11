// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Showing and changing what a layout or theme says about who made it. Built in code, so the
// share dialog, the details dialogs, Save as theme and the editor's About page all ask and show
// it the same way.

#include "ContentProvenance.h"
#include "ProvenanceFields.h"

namespace midiglass::sharing
{
    // How wide the content of every sharing and details dialog is.
    constexpr double DialogWidth = 460.0;

    // ---- the pieces the sharing dialogs are built from ----

    winrt::Microsoft::UI::Xaml::Media::Brush ThemeBrush(_In_ wchar_t const* key);

    winrt::Microsoft::UI::Xaml::Controls::TextBlock Paragraph(
        _In_ winrt::hstring const& text,
        _In_ double size = 14.0,
        _In_opt_ wchar_t const* brushKey = nullptr);

    winrt::Microsoft::UI::Xaml::Controls::TextBlock Heading(_In_ winrt::hstring const& text);

    winrt::Microsoft::UI::Xaml::Controls::TextBox Field(
        _In_ winrt::hstring const& header,
        _In_ std::wstring const& text,
        _In_ int32_t maximumLength);

    // The long date in the person's own format, or empty for no date.
    std::wstring FormatDate(_In_ int64_t fileTime);

    winrt::hstring DescribeSource(_In_ std::wstring const& type);
    winrt::hstring DescribeOversight(_In_ std::wstring const& level);

    // Everything the block says, one row each.
    winrt::Microsoft::UI::Xaml::Controls::Grid ProvenanceGrid(
        _In_ std::optional<midiapp::ContentProvenance> const& provenance,
        _In_ std::wstring const& signerName);

    // Only the parts the app writes: when, with what, from what, and the id. No rows when the
    // block says none of them.
    winrt::Microsoft::UI::Xaml::Controls::Grid ProvenanceFactsGrid(
        _In_ std::optional<midiapp::ContentProvenance> const& provenance);

    // ---- the fields ----

    struct ProvenanceForm
    {
        winrt::Microsoft::UI::Xaml::Controls::StackPanel Panel{};

        winrt::Microsoft::UI::Xaml::Controls::TextBox Author{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::TextBox Organization{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::TextBox Website{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::TextBlock WebsiteProblem{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::TextBox License{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::TextBox Version{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::ComboBox Source{ nullptr };

        // Shown only while the source involves AI.
        winrt::Microsoft::UI::Xaml::Controls::StackPanel AiPanel{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::ComboBox Oversight{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::TextBox Model{ nullptr };

        // A term the offered choices don't cover, kept as it was and offered last.
        std::wstring OtherSource{};
        std::wstring OtherOversight{};

        // Set while the form fills itself in, so a watcher doesn't take that for typing.
        bool Filling{ false };
    };

    std::shared_ptr<ProvenanceForm> BuildProvenanceForm(
        _In_ glass::ProvenanceFields const& fields,
        _In_ bool canEdit);

    void FillProvenanceForm(
        _Inout_ ProvenanceForm& form,
        _In_ glass::ProvenanceFields const& fields);

    glass::ProvenanceFields ReadProvenanceForm(_In_ ProvenanceForm const& form);

    // Under the Website field, where somebody fixing it is looking.
    void ShowWebsiteProblem(
        _In_ ProvenanceForm const& form,
        _In_ bool show);

    // The person's own name, group, website and license, where their info has one.
    void FillProvenanceFormFromProfile(
        _Inout_ ProvenanceForm& form,
        _In_ midiapp::AuthorProfile const& profile);

    bool HasAuthorProfile(_In_ midiapp::AuthorProfile const& profile) noexcept;

    // "Fill in your info", which puts the person's own info in the form and then calls `filled`.
    winrt::Microsoft::UI::Xaml::Controls::Button MakeFillInButton(
        _In_ std::shared_ptr<ProvenanceForm> const& form,
        _In_ std::function<void()> filled);

    // Calls `changed` after somebody types or picks. A text box also reports a moment after the
    // form fills it in, so `changed` has to treat a block that came out the same as no change.
    void WatchProvenanceForm(
        _In_ std::shared_ptr<ProvenanceForm> const& form,
        _In_ std::function<void()> changed);
}
