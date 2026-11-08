// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// Keyboard shortcuts, and remembering what simple mode watched.
//

#include "pch.h"
#include "MainWindow.xaml.h"

#include "App.xaml.h"

namespace native = ::midi2monitor;

namespace winrt::midi2monitor::implementation
{
    namespace
    {
        // A text box keeps its own Ctrl+C and Ctrl+L, so a comment being typed copies its text
        // rather than the selected rows.
        bool IsTextEntryFocused(_In_ xaml::XamlRoot const& root) noexcept
        {
            try
            {
                if (root == nullptr)
                {
                    return false;
                }

                auto const focused = xaml::Input::FocusManager::GetFocusedElement(root);

                return focused.try_as<controls::TextBox>() != nullptr ||
                    focused.try_as<controls::RichEditBox>() != nullptr ||
                    focused.try_as<controls::PasswordBox>() != nullptr;
            }
            catch (...)
            {
                return false;
            }
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnClearAcceleratorInvoked(xaml::Input::KeyboardAccelerator const&, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            if (IsTextEntryFocused(RootGrid().XamlRoot()))
            {
                return;
            }

            args.Handled(true);
            ConfirmClearCaptureAsync();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to handle the clear shortcut.")
    }

    _Use_decl_annotations_
    void MainWindow::OnCopyAcceleratorInvoked(xaml::Input::KeyboardAccelerator const&, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            if (IsTextEntryFocused(RootGrid().XamlRoot()))
            {
                return;
            }

            if (MessagesListView().SelectedItems().Size() == 0)
            {
                return;
            }

            args.Handled(true);
            OnCopyUmpWordsClick(nullptr, nullptr);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to handle the copy shortcut.")
    }

    bool MainWindow::ShouldRememberSelection() noexcept
    {
        try
        {
            auto const& options = App::StartupOptions();

            return options.EndpointDeviceId.empty();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to read the startup options.")

        return false;
    }

    void MainWindow::SaveSimpleSelection() noexcept
    {
        try
        {
            if (!ShouldRememberSelection())
            {
                return;
            }

            auto const endpointId = SelectedEndpointDeviceId();

            // A device that went away clears the picker. That isn't the customer choosing
            // nothing, so the last choice stays for when the device comes back.
            if (endpointId.empty())
            {
                return;
            }

            native::AppSettings::Current().LastSimpleSelection(
                std::wstring{ endpointId },
                SelectedGroupNumber(),
                SelectedChannelNumber());
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to remember the selected device.")
    }
}
