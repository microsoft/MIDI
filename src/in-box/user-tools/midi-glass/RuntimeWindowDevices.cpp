// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The half of the runtime window that talks to devices. The work itself lives in LivePlayer,
// which the editor's Try mode drives too, so a control sends exactly the same thing while it is
// being built as it does once the layout is running. What is left here is the part that is about
// this window: mapping a page item to a control, and saying on screen what the devices are doing.

#include "pch.h"
#include "RuntimeWindow.xaml.h"

#include "StringResources.h"
#include "GlassControl.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    void RuntimeWindow::StartDevices()
    {
        m_player = glass::LivePlayer::Create();

        auto weak = get_weak();

        m_player->DevicesChanged = [weak]()
            {
                if (auto strong = weak.get())
                {
                    strong->UpdateDeviceStatus();
                }
            };

        m_player->FeedbackMoved = [weak](uint32_t controlIndex, double value)
            {
                if (auto strong = weak.get())
                {
                    strong->OnFeedbackMoved(controlIndex, value);
                }
            };

        m_player->ActivitySeen = [weak](uint32_t controlIndex, glass::LivePlayer::ListenerState state)
            {
                if (auto strong = weak.get())
                {
                    strong->OnActivitySeen(controlIndex, state);
                }
            };

        m_player->BeatMoved = [weak](uint32_t controlIndex, int32_t beatInBar, double phase, bool running)
            {
                if (auto strong = weak.get())
                {
                    strong->OnBeatMoved(controlIndex, beatInBar, phase, running);
                }
            };

        m_player->LfoMoved = [weak](uint32_t controlIndex, double value, double phase, bool running)
            {
                auto strong = weak.get();

                if (strong == nullptr || strong->m_closing)
                {
                    return;
                }

                size_t itemIndex{ 0 };

                if (strong->m_renderer.TryFindItem(controlIndex, itemIndex))
                {
                    strong->m_renderer.SetSweepPosition(itemIndex, value, phase, running);
                }
            };

        m_player->TempoChanged = [weak](uint32_t controlIndex, double beatsPerMinute)
            {
                auto strong = weak.get();

                if (strong == nullptr)
                {
                    return;
                }

                size_t itemIndex{ 0 };

                if (strong->m_renderer.TryFindItem(controlIndex, itemIndex))
                {
                    strong->m_renderer.SetClockTempo(itemIndex, beatsPerMinute);
                }
            };

        // A sequence step moving a control looks exactly like a device moving one: the surface
        // follows, and nothing is sent a second time.
        m_player->ControlValueSet = [weak](uint32_t controlIndex, double value)
            {
                if (auto strong = weak.get())
                {
                    strong->OnFeedbackMoved(controlIndex, value);
                }
            };

        m_player->PageRequested = [weak](uint32_t pageIndex)
            {
                if (auto strong = weak.get())
                {
                    strong->ShowPage(pageIndex);
                }
            };

        // One owner per running layout, and the file path is what makes it unique, so the same
        // layout opened twice shares its connections instead of doubling them.
        m_player->Start(m_document, m_dispatcher, m_ownerId);
    }

    void RuntimeWindow::UpdateDeviceStatus()
    {
        try
        {
            if (m_player == nullptr)
            {
                return;
            }

            auto const devices = m_player->Devices();

            if (devices.empty())
            {
                DeviceStatusText().Text(resources::GetString(L"RuntimeNoDevices"));
                return;
            }

            std::wstring missing{};
            size_t available{ 0 };

            for (auto const& device : devices)
            {
                if (device.IsAvailable)
                {
                    available++;
                }
                else
                {
                    if (!missing.empty())
                    {
                        missing += L", ";
                    }

                    missing += device.Name;
                }
            }

            if (missing.empty())
            {
                DeviceStatusText().Text(resources::FormatString(
                    L"RuntimeAllDevicesFormat", static_cast<int32_t>(available)));
            }
            else
            {
                // Named, because "a device is missing" is not something anybody can act on.
                DeviceStatusText().Text(resources::FormatString(L"RuntimeMissingDevicesFormat", missing));
            }

            MarkUnreachableControls(devices);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the device status.")
    }

    // A control whose device is not here is struck through on the surface. The status line says
    // which device went; this says which controls have gone quiet because of it, which is the
    // part somebody can act on mid set.
    _Use_decl_annotations_
    void RuntimeWindow::MarkUnreachableControls(std::vector<glass::ResolvedDevice> const& devices)
    {
        try
        {
            if (m_pageIndex >= m_document.Pages.size())
            {
                return;
            }

            std::vector<std::wstring> gone{};

            for (auto const& device : devices)
            {
                if (!device.IsAvailable)
                {
                    gone.push_back(device.Name);
                }
            }

            auto const& page = m_document.Pages[m_pageIndex];

            for (size_t index = 0; index < page.Controls.size() && index < m_renderer.ItemCount(); ++index)
            {
                auto const& control = page.Controls[index];

                auto unreachable = false;

                for (auto const& message : control.Messages)
                {
                    if (!message.DeviceName.empty() &&
                        std::find(gone.begin(), gone.end(), message.DeviceName) != gone.end())
                    {
                        unreachable = true;
                        break;
                    }
                }

                m_renderer.SetUnavailable(index, unreachable);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to mark the controls whose device is missing.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnControlValueChanged(size_t itemIndex, double value, bool isFinal)
    {
        m_renderer.SetValue(itemIndex, value);

        if (m_player != nullptr)
        {
            m_player->ValueChanged(m_renderer.ControlIndexOf(itemIndex), value, isFinal);
        }
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnControlValueYChanged(size_t itemIndex, double value, bool isFinal)
    {
        m_renderer.SetValueY(itemIndex, value);

        if (m_player != nullptr)
        {
            m_player->ValueYChanged(m_renderer.ControlIndexOf(itemIndex), value, isFinal);
        }
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnControlKeyChanged(
        size_t itemIndex,
        int32_t key,
        double velocity,
        bool isDown)
    {
        if (isDown)
        {
            m_renderer.Bloom(itemIndex);
        }

        if (m_player != nullptr)
        {
            m_player->KeyChanged(m_renderer.ControlIndexOf(itemIndex), key, velocity, isDown);
        }
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnControlSetDirectly(size_t itemIndex, double value)
    {
        // Assistive technology setting a value is one discrete change, not a drag, so it is a
        // whole gesture: touched, moved to here, released. Going straight to the release would
        // find nothing held back and send nothing at all.
        OnControlValueChanged(itemIndex, value, false);
        OnControlValueChanged(itemIndex, value, true);
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnControlSwitched(size_t itemIndex, bool isOn, double velocity)
    {
        m_renderer.SetValue(itemIndex, isOn ? 1.0 : 0.0);

        // A time display counts again from zero when it is tapped. Nothing is sent: it is
        // there for the person on stage, not for the desk.
        if (m_renderer.KindAt(itemIndex) == glass::ControlKind::TimeDisplay)
        {
            if (isOn)
            {
                m_renderer.ResetElapsed(itemIndex);
            }

            return;
        }

        if (m_player != nullptr)
        {
            m_player->Switched(m_renderer.ControlIndexOf(itemIndex), isOn, velocity);
        }
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnControlTouched(size_t itemIndex, bool isTouched)
    {
        if (isTouched)
        {
            m_renderer.Bloom(itemIndex);
        }

        m_renderer.SetTouched(itemIndex, isTouched);

        if (m_player != nullptr)
        {
            m_player->Touched(m_renderer.ControlIndexOf(itemIndex), isTouched);
        }
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnFeedbackMoved(uint32_t controlIndex, double value)
    {
        if (m_closing)
        {
            return;
        }

        size_t itemIndex{ 0 };

        // A control on another page is still tracked by the engine; it simply has nothing on
        // screen to move.
        if (!m_renderer.TryFindItem(controlIndex, itemIndex))
        {
            return;
        }

        m_renderer.SetValue(itemIndex, value);
        m_renderer.BloomFeedback(itemIndex);

        if (auto element = m_renderer.ElementAt(itemIndex))
        {
            winrt::get_self<implementation::GlassControl>(element)->SetValueDirect(value);
        }
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnActivitySeen(
        uint32_t controlIndex,
        glass::LivePlayer::ListenerState state)
    {
        if (m_closing)
        {
            return;
        }

        size_t itemIndex{ 0 };

        if (!m_renderer.TryFindItem(controlIndex, itemIndex))
        {
            return;
        }

        switch (state)
        {
        case glass::LivePlayer::ListenerState::On:
            // Latched. "Is the sequencer running" is a state, not an event, so it stays lit
            // rather than decaying the way a blink does.
            m_renderer.SetValue(itemIndex, 1.0);
            break;

        case glass::LivePlayer::ListenerState::Off:
            m_renderer.SetValue(itemIndex, 0.0);
            m_renderer.ClearBloom(itemIndex);
            break;

        default:
            // A blink has no value to carry, so the light is the whole message. It goes out on
            // its own after "stays lit for", which is what makes it read as a blink rather than
            // a light left on.
            m_renderer.FlashFeedback(itemIndex);
            break;
        }
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnBeatMoved(
        uint32_t controlIndex,
        int32_t beatInBar,
        double phase,
        bool running)
    {
        if (m_closing)
        {
            return;
        }

        size_t itemIndex{ 0 };

        if (!m_renderer.TryFindItem(controlIndex, itemIndex))
        {
            return;
        }

        m_renderer.SetBeat(itemIndex, beatInBar, phase, running);
    }
}
