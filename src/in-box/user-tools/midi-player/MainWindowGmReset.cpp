// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// GM Reset: General MIDI System On, sent from the button beside the output picker, and before
// each file when the customer turns that on. Many files never choose their sounds, piano
// recordings especially, and an instrument that keeps its state between files then plays them
// with whatever the last file left behind.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "StringResources.h"

namespace native = ::midiplayer;
namespace res = ::midiplayer::resources;

namespace winrt::midiplayer::implementation
{
    namespace
    {
        // GM2 4.9.1 gives an instrument up to 100 ms to finish a reset. The music, and anything
        // the player restores at the starting point, waits that long after it.
        constexpr uint32_t GmResetSettleMilliseconds = 100;

        // F0 7E 7F 09 01 F7: Universal Non-Real Time, every device, General MIDI, GM System On.
        // One complete sysex7 packet in the chosen group.
        std::vector<uint32_t> GmSystemOnWords(_In_ uint8_t groupIndex)
        {
            return
            {
                (0x3u << 28) | (static_cast<uint32_t>(groupIndex & 0x0F) << 24) | (0x0u << 20) | (0x4u << 16) | (0x7Eu << 8) | 0x7Fu,
                (0x09u << 24) | (0x01u << 16),
            };
        }
    }

    _Use_decl_annotations_
    void MainWindow::QueueGmResetBeforeNextStart(uint8_t groupIndex) noexcept
    {
        try
        {
            if (native::AppSettings::Current().GmResetBeforeEachFile())
            {
                m_engine.SendBeforeNextStart(GmSystemOnWords(groupIndex), GmResetSettleMilliseconds);
            }
            else
            {
                m_engine.SendBeforeNextStart({}, 0);
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to set up the GM Reset before the file.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnGmResetClick(foundation::IInspectable sender, xaml::RoutedEventArgs args)
    {
        auto strong = get_strong();

        (void)sender;
        (void)args;

        try
        {
            // Ignored while a file is being loaded: that opens the output itself, and could be
            // switching it to a newly chosen one.
            if (m_gmResetSending || m_busy)
            {
                co_return;
            }

            auto const endpointDeviceId = SelectedEndpointDeviceId();

            if (endpointDeviceId.empty())
            {
                ShowStatus(res::GetString(L"StatusChooseEndpoint"), controls::InfoBarSeverity::Warning);
                co_return;
            }

            auto const groupIndex = native::AppSettings::Current().GroupIndex();

            // Closed first, so the message is announced again when the button is pressed twice.
            ClearStatus();

            // Sent now, it would land among notes already handed to the service, which would then
            // play on the piano. So the music stops the way a seek stops it, the reset goes out
            // once that silence has landed, and the file carries on from here with its own sounds.
            if (m_engine.State() == native::PlaybackState::Playing &&
                m_engine.EndpointDeviceId() == endpointDeviceId)
            {
                m_engine.SendBeforeNextStart(GmSystemOnWords(groupIndex), GmResetSettleMilliseconds);
                m_engine.SeekToMicroseconds(m_engine.Position().Microseconds);

                ShowStatus(res::GetString(L"StatusGmResetSent"), controls::InfoBarSeverity::Success);
                co_return;
            }

            m_gmResetSending = true;

            bool sent{ false };

            co_await native::RunOnBackgroundAsync(
                [this, endpointDeviceId, groupIndex, &sent]()
                {
                    if (!EnsureSession())
                    {
                        return;
                    }

                    // The output the music would go to, which is often not open yet: the reset
                    // is most useful before the first file plays.
                    if (m_engine.Open(m_session, endpointDeviceId) != native::OpenResult::Success)
                    {
                        return;
                    }

                    auto const connection = m_engine.Connection();

                    if (connection == nullptr)
                    {
                        return;
                    }

                    auto const words = GmSystemOnWords(groupIndex);

                    auto const result = connection.SendMultipleMessagesWordArray(
                        midi2::MidiClock::TimestampConstantSendImmediately(),
                        0,
                        static_cast<uint32_t>(words.size()),
                        winrt::array_view<uint32_t const>{ words.data(), words.data() + words.size() });

                    sent = midi2::MidiEndpointConnection::SendMessageSucceeded(result);
                });

            m_gmResetSending = false;

            ShowStatus(
                res::GetString(sent ? L"StatusGmResetSent" : L"StatusGmResetFailed"),
                sent ? controls::InfoBarSeverity::Success : controls::InfoBarSeverity::Error);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to send the GM Reset.")

        m_gmResetSending = false;
    }

    xaml::UIElement MainWindow::BuildGmResetSetting() noexcept
    {
        try
        {
            auto const resources = xaml::Application::Current().Resources();

            controls::StackPanel panel{};
            panel.Spacing(8);

            auto const label = res::GetString(L"SettingsGmResetBeforeEachFile");
            auto const hintText = res::GetString(L"SettingsGmResetBeforeEachFileHint");

            // A header that wraps, because a translation can run a good deal longer than this.
            controls::TextBlock header{};
            header.Text(label);
            header.TextWrapping(xaml::TextWrapping::Wrap);

            controls::ToggleSwitch toggle{};
            toggle.Header(header);
            toggle.IsOn(native::AppSettings::Current().GmResetBeforeEachFile());

            // The header is an element rather than text, so the name is given outright.
            xaml::Automation::AutomationProperties::SetName(toggle, label);
            xaml::Automation::AutomationProperties::SetHelpText(toggle, hintText);
            xaml::Automation::AutomationProperties::SetAutomationId(toggle, L"GmResetBeforeEachFileToggle");

            toggle.Toggled([weak = get_weak()](foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
                {
                    try
                    {
                        auto const control = sender.try_as<controls::ToggleSwitch>();

                        if (control == nullptr)
                        {
                            return;
                        }

                        native::AppSettings::Current().GmResetBeforeEachFile(control.IsOn());

                        auto strong = weak.get();

                        if (strong == nullptr)
                        {
                            return;
                        }

                        // A file loaded but not started yet counts as the next file. One that is
                        // playing or paused has already started, so the change waits for the next.
                        if (!control.IsOn() || strong->m_engine.State() == native::PlaybackState::Stopped)
                        {
                            strong->QueueGmResetBeforeNextStart(native::AppSettings::Current().GroupIndex());
                        }
                    }
                    MIDI_PLAYER_CATCH_AND_LOG(L"Unable to change the GM Reset setting.")
                });

            panel.Children().Append(toggle);

            controls::TextBlock hint{};
            hint.Text(hintText);
            hint.TextWrapping(xaml::TextWrapping::Wrap);
            hint.Foreground(resources.Lookup(winrt::box_value(L"TextFillColorTertiaryBrush")).as<media::Brush>());
            panel.Children().Append(hint);

            return panel;
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to build the GM Reset setting.")

        return nullptr;
    }
}
