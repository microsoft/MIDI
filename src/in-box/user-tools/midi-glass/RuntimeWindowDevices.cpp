// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The half of the runtime window that talks to devices: resolving the layout's device table,
// opening connections, sending from the pointer handler, and taking feedback back in.

#include "pch.h"
#include "RuntimeWindow.xaml.h"

#include "StringResources.h"
#include "GlassControl.h"

#include <ump_helpers.h>

namespace internal = ::WindowsMidiServicesInternal;
namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        uint64_t NowMilliseconds() noexcept
        {
            return static_cast<uint64_t>(::GetTickCount64());
        }
    }

    void RuntimeWindow::StartDevices()
    {
        m_throttles.assign(m_document.ControlCount(), glass::ValueThrottle{});

        size_t index{ 0 };

        for (auto const& page : m_document.Pages)
        {
            for (auto const& control : page.Controls)
            {
                if (index < m_throttles.size())
                {
                    m_throttles[index].SetMinimumInterval(
                        static_cast<uint32_t>(std::max(0, control.SendIntervalMilliseconds)));
                }

                index++;
            }
        }

        auto weak = get_weak();

        m_devices.SetChangedHandler([weak]()
            {
                auto strong = weak.get();

                if (strong == nullptr || strong->m_dispatcher == nullptr)
                {
                    return;
                }

                // The watcher calls on its own thread. Nothing below the window layer calls up
                // into the UI, so the marshalling happens here.
                strong->m_dispatcher.TryEnqueue([weak]()
                    {
                        if (auto inner = weak.get())
                        {
                            inner->ReopenConnections();
                        }
                    });
            });

        m_devices.SetDocument(m_document);

        // Starting the watcher blocks on the service, so it never runs on the UI thread.
        std::thread([weak]()
            {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);

                if (auto strong = weak.get())
                {
                    strong->m_devices.Start();
                }

                winrt::uninit_apartment();
            }).detach();

        ReopenConnections();
    }

    void RuntimeWindow::ReopenConnections()
    {
        if (m_closing)
        {
            return;
        }

        // The watcher fires once per endpoint on the machine at startup, and again whenever
        // anything at all is plugged in. Rebuilding connections for a device this layout does
        // not use would interrupt what it is playing, so the work is skipped unless what this
        // layout resolved to actually changed.
        std::wstring signature{};

        for (auto const& device : m_devices.Devices())
        {
            signature += device.Name;
            signature += L'\x1';
            signature += device.EndpointDeviceId;
            signature += L'\x1';
            signature += device.IsAvailable ? L'1' : L'0';
            signature += L'\n';
        }

        if (signature == m_destinationSignature && !m_sendTable.empty())
        {
            return;
        }

        m_destinationSignature = signature;

        // Every device name resolves to an index here, once, so nothing on the path a finger
        // takes ever compares a string.
        m_engine.Prepare(m_document, m_devices.BuildDestinations());

        UpdateDeviceStatus();

        std::vector<std::wstring> endpointIds{};
        std::vector<uint16_t> groupMasks{};

        m_devices.BuildOutputRequests(endpointIds, groupMasks);

        std::vector<glass::OutputRequest> requests{};
        requests.reserve(endpointIds.size());

        for (size_t i = 0; i < endpointIds.size(); ++i)
        {
            requests.push_back({ endpointIds[i], i < groupMasks.size() ? groupMasks[i] : uint16_t{ 0 } });
        }

        auto weak = get_weak();
        auto const ownerId = m_ownerId;

        std::thread([weak, ownerId, requests]()
            {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);

                std::vector<winrt::com_ptr<IMidiEndpointConnectionRaw>> table{};

                glass::FeedbackHandler handler =
                    [weak](std::wstring const&, uint64_t timestamp, uint32_t wordCount, uint32_t const* words)
                    {
                        auto strong = weak.get();

                        if (strong == nullptr)
                        {
                            return;
                        }

                        strong->OnFeedbackWords(timestamp, wordCount, words);
                    };

                glass::OutputRouter::Current().Open(ownerId, requests, handler, table);

                auto strong = weak.get();

                if (strong == nullptr || strong->m_dispatcher == nullptr)
                {
                    winrt::uninit_apartment();
                    return;
                }

                // The send table is only ever read on the UI thread, which is why the hot path
                // needs no lock. Swapping it whole is what keeps that true.
                strong->m_dispatcher.TryEnqueue([weak, table]()
                    {
                        auto inner = weak.get();

                        if (inner == nullptr || inner->m_closing)
                        {
                            return;
                        }

                        inner->m_sendTable = table;
                        inner->UpdateDeviceStatus();
                        inner->SendStartupValues();
                    });

                winrt::uninit_apartment();
            }).detach();
    }

    void RuntimeWindow::UpdateDeviceStatus()
    {
        try
        {
            auto const devices = m_devices.Devices();

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
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the device status.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::SendPrepared(uint32_t count) noexcept
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            auto const& send = m_sends[i];

            if (send.DestinationIndex < 0 ||
                static_cast<size_t>(send.DestinationIndex) >= m_sendTable.size())
            {
                continue;
            }

            auto const& connection = m_sendTable[static_cast<size_t>(send.DestinationIndex)];

            if (connection == nullptr || send.WordCount == 0)
            {
                continue;
            }

            // Zero is now. Measured at a third of a microsecond from the UI thread, which is why
            // this is done in the pointer handler rather than queued to a render tick.
            connection->SendMidiMessagesRaw(0, send.WordCount, send.Words);
        }
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnControlValueChanged(size_t itemIndex, double value, bool isFinal)
    {
        auto const controlIndex = m_renderer.ControlIndexOf(itemIndex);

        m_renderer.SetValue(itemIndex, value);

        if (controlIndex >= m_throttles.size())
        {
            return;
        }

        auto& throttle = m_throttles[controlIndex];

        if (isFinal)
        {
            // The last value is always sent. Without it a fader settles a few units from where
            // the finger left it and the surface and the desk disagree for the rest of the set.
            double pending{ 0.0 };

            if (!throttle.Release(pending))
            {
                return;
            }

            value = pending;
        }
        else if (!throttle.ShouldSend(value, NowMilliseconds()))
        {
            return;
        }

        SendPrepared(m_engine.Evaluate(
            controlIndex, glass::MessageTrigger::Changes, value, m_sends));
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
        auto const controlIndex = m_renderer.ControlIndexOf(itemIndex);

        m_renderer.SetValue(itemIndex, isOn ? 1.0 : 0.0);

        // Nothing but a continuous control is ever throttled. Rate limiting a note on would be a
        // defect, not a feature.
        SendPrepared(m_engine.Evaluate(
            controlIndex,
            isOn ? glass::MessageTrigger::TurnsOn : glass::MessageTrigger::TurnsOff,
            isOn ? 1.0 : 0.0,
            m_sends));

        // A control that only declares "changes" still has to do something when a pad is hit.
        SendPrepared(m_engine.Evaluate(
            controlIndex, glass::MessageTrigger::Changes, isOn ? 1.0 : 0.0, m_sends));
    }

    void RuntimeWindow::SendStartupValues()
    {
        // A layout initializes once per run. Replaying it because an unrelated device was
        // plugged in would push a whole desk back to its opening positions mid set.
        if (m_document.SuppressAllStartupValues || m_startupValuesSent)
        {
            return;
        }

        // In keyboard order, which is the order the person building the layout could see and fix,
        // rather than the order the controls happen to sit in the file.
        std::array<glass::PreparedSend, 64> sends{};

        auto const written = m_engine.EvaluateStartupValues(sends);

        uint32_t sent{ 0 };

        for (uint32_t i = 0; i < written; ++i)
        {
            auto const& send = sends[i];

            if (send.DestinationIndex < 0 ||
                static_cast<size_t>(send.DestinationIndex) >= m_sendTable.size())
            {
                continue;
            }

            auto const& connection = m_sendTable[static_cast<size_t>(send.DestinationIndex)];

            if (connection != nullptr && send.WordCount != 0)
            {
                connection->SendMidiMessagesRaw(0, send.WordCount, send.Words);
                sent++;
            }
        }

        // Marked only once something actually went out. The devices resolve on a watcher thread
        // a moment after the window opens, so the first pass through here usually has nothing
        // connected yet, and claiming the layout had been initialized then would mean it never
        // was.
        m_startupValuesSent = sent > 0;
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnFeedbackWords(
        uint64_t timestamp,
        uint32_t wordCount,
        uint32_t const* words)
    {
        UNREFERENCED_PARAMETER(timestamp);

        if (words == nullptr || wordCount == 0 || m_dispatcher == nullptr)
        {
            return;
        }

        // This is a service callback thread and the buffer belongs to the caller, so what is
        // wanted is resolved here and only the answer is marshalled.
        std::vector<std::pair<size_t, double>> moves{};

        uint32_t position{ 0 };

        while (position < wordCount)
        {
            auto const length = internal::GetUmpLengthInMidiWordsFromFirstWord(words[position]);

            if (length == 0 || position + length > wordCount)
            {
                break;
            }

            size_t controlIndex{ 0 };
            double value{ 0.0 };

            if (m_engine.TryResolveFeedback(words + position, length, controlIndex, value))
            {
                moves.emplace_back(controlIndex, value);
            }

            position += length;
        }

        if (moves.empty())
        {
            return;
        }

        auto weak = get_weak();

        m_dispatcher.TryEnqueue([weak, moves]()
            {
                auto strong = weak.get();

                if (strong == nullptr || strong->m_closing)
                {
                    return;
                }

                for (auto const& [controlIndex, value] : moves)
                {
                    size_t itemIndex{ 0 };

                    // A control on another page is still tracked by the engine; it simply has
                    // nothing on screen to move.
                    if (!strong->m_renderer.TryFindItem(static_cast<uint32_t>(controlIndex), itemIndex))
                    {
                        continue;
                    }

                    strong->m_renderer.SetValue(itemIndex, value);
                    strong->m_renderer.Bloom(itemIndex);

                    if (auto element = strong->m_renderer.ElementAt(itemIndex))
                    {
                        winrt::get_self<implementation::GlassControl>(element)->SetValueDirect(value);
                    }
                }
            });
    }
}
