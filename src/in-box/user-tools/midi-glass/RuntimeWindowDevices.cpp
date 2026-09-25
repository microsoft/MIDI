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
    void RuntimeWindow::OnControlSetDirectly(size_t itemIndex, double value)
    {
        // Assistive technology setting a value is one discrete change, not a drag, so it is a
        // whole gesture: touched, moved to here, released. Going straight to the release would
        // find nothing held back and send nothing at all.
        OnControlValueChanged(itemIndex, value, false);
        OnControlValueChanged(itemIndex, value, true);
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnControlSwitched(size_t itemIndex, bool isOn)
    {
        m_renderer.SetValue(itemIndex, isOn ? 1.0 : 0.0);

        if (m_player != nullptr)
        {
            m_player->Switched(m_renderer.ControlIndexOf(itemIndex), isOn);
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
        m_renderer.Bloom(itemIndex);

        if (auto element = m_renderer.ElementAt(itemIndex))
        {
            winrt::get_self<implementation::GlassControl>(element)->SetValueDirect(value);
        }
    }
}
