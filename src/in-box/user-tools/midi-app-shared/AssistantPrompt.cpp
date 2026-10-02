// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AssistantPrompt.h"
#include "EndpointCatalog.h"

#include <winrt/Windows.ApplicationModel.DataTransfer.h>

namespace midiapp
{
    namespace wux = ::winrt::Microsoft::UI::Xaml;
    namespace wuxc = ::winrt::Microsoft::UI::Xaml::Controls;
    namespace datatransfer = ::winrt::Windows::ApplicationModel::DataTransfer;

    _Use_decl_annotations_
    std::wstring FormatPromptList(std::vector<std::wstring> const& names)
    {
        std::wstring list{};

        for (auto const& name : names)
        {
            auto const clean = SanitizeStoredString(name);

            if (clean.empty())
            {
                continue;
            }

            if (!list.empty())
            {
                list += L"\r\n";
            }

            list += L"- ";
            list += clean;
        }

        return list;
    }

    std::vector<std::wstring> EndpointNamesForPrompt()
    {
        std::vector<std::wstring> names{};

        for (auto const& endpoint : EndpointCatalog::Current().Snapshot())
        {
            names.push_back(endpoint.Name);
        }

        std::sort(names.begin(), names.end(),
            [](std::wstring const& left, std::wstring const& right)
            {
                return ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_LESS_THAN;
            });

        return names;
    }

    _Use_decl_annotations_
    winrt::Windows::Foundation::IAsyncAction ShowAssistantPromptAsync(
        wux::XamlRoot root,
        AssistantPromptStrings strings,
        winrt::hstring prompt,
        winrt::Windows::Foundation::Uri guide)
    {
        wuxc::TextBlock message{};
        message.Text(strings.Message);
        message.TextWrapping(wux::TextWrapping::Wrap);

        wuxc::TextBox promptBox{};
        promptBox.Header(winrt::box_value(strings.PromptHeader));
        promptBox.Text(prompt);
        promptBox.IsReadOnly(true);
        promptBox.AcceptsReturn(true);
        promptBox.TextWrapping(wux::TextWrapping::Wrap);
        promptBox.Height(240.0);
        wuxc::ScrollViewer::SetVerticalScrollBarVisibility(promptBox, wuxc::ScrollBarVisibility::Auto);

        wuxc::HyperlinkButton guideLink{};
        guideLink.Content(winrt::box_value(strings.GuideLink));
        guideLink.NavigateUri(guide);

        wuxc::StackPanel panel{};
        panel.Spacing(12.0);
        panel.Width(480.0);
        panel.Children().Append(message);
        panel.Children().Append(promptBox);
        panel.Children().Append(guideLink);

        wuxc::ContentDialog dialog{};
        dialog.XamlRoot(root);
        dialog.Title(winrt::box_value(strings.Title));
        dialog.Content(panel);
        dialog.PrimaryButtonText(strings.CopyButton);
        dialog.CloseButtonText(strings.CloseButton);
        dialog.DefaultButton(wuxc::ContentDialogButton::Primary);

        dialog.PrimaryButtonClick([strings, prompt](
            wuxc::ContentDialog const& sender,
            wuxc::ContentDialogButtonClickEventArgs const& args)
            {
                auto label = strings.CopiedButton;

                try
                {
                    args.Cancel(true);

                    datatransfer::DataPackage package{};
                    package.SetText(prompt);

                    datatransfer::Clipboard::SetContent(package);
                }
                catch (...)
                {
                    // Another app can be holding the clipboard open. The button says so, and
                    // pressing it again tries again.
                    label = strings.CopyFailedButton;
                }

                try
                {
                    sender.PrimaryButtonText(label);
                }
                catch (...)
                {
                }
            });

        co_await dialog.ShowAsync();
    }
}
