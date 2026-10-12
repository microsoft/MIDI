// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AssistantPrompt.h"
#include "DialogSizing.h"
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

    namespace
    {
        // The message, the link and the spacing take about this much of the dialog's height, and
        // the looking bar about this much more while it shows.
        constexpr double OtherContentHeight = 160.0;
        constexpr double LookingBarHeight = 112.0;

        // As much of the prompt as the window has room for.
        double PromptBoxHeight(_In_ double spaceHeight, _In_ bool withLookingBar) noexcept
        {
            auto const others = OtherContentHeight + (withLookingBar ? LookingBarHeight : 0.0);

            return std::clamp(spaceHeight - others, 240.0, 640.0);
        }

        // The parts of the dialog that change while it's open.
        struct AssistantPromptView
        {
            AssistantPromptStrings Strings{};
            std::function<winrt::hstring(bool)> BuildPrompt{};
            winrt::hstring Prompt{};
            double SpaceHeight{ 0.0 };

            wuxc::StackPanel Panel{};
            wuxc::TextBox PromptBox{};
            wuxc::ContentDialog Dialog{};
            wuxc::InfoBar Looking{ nullptr };

            void ShowPrompt(_In_ winrt::hstring const& prompt)
            {
                if (prompt == Prompt)
                {
                    return;
                }

                Prompt = prompt;
                PromptBox.Text(prompt);

                // What was copied before isn't what the box shows now.
                Dialog.PrimaryButtonText(Strings.CopyButton);
            }

            void Refresh()
            {
                auto const stillLooking = !EndpointCatalog::Current().HasEnumerated();

                ShowPrompt(BuildPrompt(stillLooking));

                Looking.IsOpen(stillLooking);
                Looking.Visibility(stillLooking ? wux::Visibility::Visible : wux::Visibility::Collapsed);
                PromptBox.Height(PromptBoxHeight(SpaceHeight, stillLooking));
                Dialog.IsPrimaryButtonEnabled(!stillLooking);
            }
        };

        // Ends the subscription however the dialog ends.
        struct EndpointChangeSubscription
        {
            uint64_t Token{ 0 };

            ~EndpointChangeSubscription()
            {
                EndpointCatalog::Current().RemoveChangedHandler(Token);
            }
        };

        std::shared_ptr<AssistantPromptView> MakeAssistantPromptView(
            _In_ wux::XamlRoot const& root,
            _In_ AssistantPromptStrings const& strings,
            _In_ winrt::Windows::Foundation::Uri const& guide)
        {
            auto const space = DialogContentSpace(root);

            auto view = std::make_shared<AssistantPromptView>();
            view->Strings = strings;
            view->SpaceHeight = space.Height;

            wuxc::TextBlock message{};
            message.Text(strings.Message);
            message.TextWrapping(wux::TextWrapping::Wrap);

            auto const& promptBox = view->PromptBox;
            promptBox.Header(winrt::box_value(strings.PromptHeader));

            // Multi-line before the text goes in: a single-line box keeps only the first line.
            promptBox.AcceptsReturn(true);
            promptBox.TextWrapping(wux::TextWrapping::Wrap);
            promptBox.IsReadOnly(true);
            promptBox.Height(PromptBoxHeight(space.Height, false));
            wuxc::ScrollViewer::SetVerticalScrollBarVisibility(promptBox, wuxc::ScrollBarVisibility::Auto);

            wuxc::HyperlinkButton guideLink{};
            guideLink.Content(winrt::box_value(strings.GuideLink));
            guideLink.NavigateUri(guide);

            auto const& panel = view->Panel;
            panel.Spacing(12.0);
            panel.Width(std::clamp(static_cast<double>(space.Width), 480.0, 760.0));
            panel.Children().Append(message);
            panel.Children().Append(promptBox);
            panel.Children().Append(guideLink);

            auto const& dialog = view->Dialog;
            dialog.XamlRoot(root);
            LetDialogGrow(dialog);
            dialog.Title(winrt::box_value(strings.Title));
            dialog.Content(panel);
            dialog.PrimaryButtonText(strings.CopyButton);
            dialog.CloseButtonText(strings.CloseButton);
            dialog.DefaultButton(wuxc::ContentDialogButton::Primary);

            // Weak, so the dialog doesn't keep alive the view that holds it.
            std::weak_ptr<AssistantPromptView> weakView = view;

            dialog.PrimaryButtonClick([weakView](
                wuxc::ContentDialog const& sender,
                wuxc::ContentDialogButtonClickEventArgs const& args)
                {
                    auto const strongView = weakView.lock();

                    if (strongView == nullptr)
                    {
                        return;
                    }

                    auto label = strongView->Strings.CopiedButton;

                    try
                    {
                        args.Cancel(true);

                        datatransfer::DataPackage package{};
                        package.SetText(strongView->Prompt);

                        datatransfer::Clipboard::SetContent(package);
                    }
                    catch (...)
                    {
                        // Another app can be holding the clipboard open. The button says so, and
                        // pressing it again tries again.
                        label = strongView->Strings.CopyFailedButton;
                    }

                    try
                    {
                        sender.PrimaryButtonText(label);
                    }
                    catch (...)
                    {
                    }
                });

            return view;
        }
    }

    _Use_decl_annotations_
    winrt::Windows::Foundation::IAsyncAction ShowAssistantPromptAsync(
        wux::XamlRoot root,
        AssistantPromptStrings strings,
        winrt::hstring prompt,
        winrt::Windows::Foundation::Uri guide)
    {
        auto const view = MakeAssistantPromptView(root, strings, guide);

        view->ShowPrompt(prompt);

        co_await view->Dialog.ShowAsync();
    }

    _Use_decl_annotations_
    winrt::Windows::Foundation::IAsyncAction ShowAssistantPromptAsync(
        wux::XamlRoot root,
        AssistantPromptStrings strings,
        std::function<winrt::hstring(bool stillLooking)> buildPrompt,
        winrt::Windows::Foundation::Uri guide)
    {
        auto const view = MakeAssistantPromptView(root, strings, guide);
        view->BuildPrompt = std::move(buildPrompt);

        wuxc::ProgressBar progress{};
        progress.IsIndeterminate(true);

        view->Looking = wuxc::InfoBar{};
        view->Looking.IsClosable(false);
        view->Looking.Severity(wuxc::InfoBarSeverity::Informational);
        view->Looking.Title(strings.LookingTitle);
        view->Looking.Message(strings.LookingMessage);
        view->Looking.Content(progress);

        view->Panel.Children().InsertAt(1, view->Looking);

        // Only a weak reference crosses threads, so the XAML objects are always let go on this one.
        std::weak_ptr<AssistantPromptView> weakView = view;

        EndpointChangeSubscription subscription{};

        // Before the first build, so a look that ends in between still reaches the dialog.
        subscription.Token = EndpointCatalog::Current().AddChangedHandler(
            [weakView, queue = view->Dialog.DispatcherQueue()]()
            {
                if (queue == nullptr)
                {
                    return;
                }

                queue.TryEnqueue([weakView]()
                    {
                        try
                        {
                            if (auto const strongView = weakView.lock())
                            {
                                strongView->Refresh();
                            }
                        }
                        catch (...)
                        {
                            ReportEndpointError(L"Unable to refresh the AI assistant prompt.");
                        }
                    });
            });

        view->Refresh();

        co_await view->Dialog.ShowAsync();
    }
}
