// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiapp
{
    // Captions come from the calling app's own resources, as with AppearanceStrings.
    struct AssistantPromptStrings
    {
        winrt::hstring Title{};
        winrt::hstring Message{};
        winrt::hstring PromptHeader{};
        winrt::hstring GuideLink{};
        winrt::hstring CopyButton{};
        winrt::hstring CopiedButton{};
        winrt::hstring CopyFailedButton{};
        winrt::hstring CloseButton{};

        // Shown until the first look at the devices is done.
        winrt::hstring LookingTitle{};
        winrt::hstring LookingMessage{};
    };

    // One "- name" line per name. Each name is sanitized, so a device name can't add lines of its own.
    std::wstring FormatPromptList(_In_ std::vector<std::wstring> const& names);

    // The names Windows shows for the endpoints present now, in alphabetical order.
    std::vector<std::wstring> EndpointNamesForPrompt();

    // The customer sees the whole prompt before anything leaves the app. It's rebuilt when the
    // device list changes; while stillLooking is true it leaves the devices out and Copy waits.
    winrt::Windows::Foundation::IAsyncAction ShowAssistantPromptAsync(
        _In_ winrt::Microsoft::UI::Xaml::XamlRoot root,
        _In_ AssistantPromptStrings strings,
        _In_ std::function<winrt::hstring(bool stillLooking)> buildPrompt,
        _In_ winrt::Windows::Foundation::Uri guide);
}
