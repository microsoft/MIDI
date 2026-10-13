// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ControllerStepRunner.h"
#include "ControllerHub.h"
#include "HidHub.h"

#include <midi_timestamp.h>

namespace internal = ::WindowsMidiServicesInternal;

namespace midipatchbay
{
    _Use_decl_annotations_
    ProgramPosition SharedProgramPosition(std::wstring const& patchKey, uint8_t group, uint8_t channel)
    {
        static std::mutex lock{};
        static std::map<std::wstring, ProgramPosition> positions{};

        auto const key = patchKey + L'|' + std::to_wstring(group & 0x0F) + L'|' + std::to_wstring(channel & 0x0F);

        std::lock_guard guard{ lock };

        auto& position = positions[key];

        if (position == nullptr)
        {
            position = std::make_shared<std::atomic<int32_t>>(-1);
        }

        return position;
    }

    _Use_decl_annotations_
    ControllerStepRunner::ControllerStepRunner(
        BlockKind kind,
        std::wstring patchKey,
        BlockSettings const& settings,
        Send send) :
        m_kind(kind),
        m_patchKey(std::move(patchKey)),
        m_send(std::move(send)),
        m_button(settings.ControllerButton),
        m_axis(settings.ControllerAxis),
        m_buttonMachine([key = m_patchKey](uint8_t group, uint8_t channel) { return SharedProgramPosition(key, group, channel); })
    {
    }

    ControllerStepRunner::~ControllerStepRunner()
    {
        Stop();
    }

    bool ControllerStepRunner::Start() noexcept
    {
        try
        {
            if (m_token != 0)
            {
                return true;
            }

            auto const isButton = m_kind == BlockKind::ControllerButton;

            m_readsDevice = isButton ? m_button.ReadsDevice() : m_axis.ReadsDevice();

            if (m_readsDevice)
            {
                m_token = HidHub::Current().Listen(isButton ? m_button.Device : m_axis.Device,
                    [this](uint64_t nowMilliseconds, uint64_t timestamp, HidReading const& reading)
                    {
                        OnHidReading(nowMilliseconds, timestamp, reading);
                    });

                return m_token != 0;
            }

            auto const player = isButton ? m_button.Player : m_axis.Player;

            m_token = ControllerHub::Current().Listen(player,
                [this](uint8_t, uint64_t nowMilliseconds, uint64_t timestamp, ControllerReading const& reading)
                {
                    OnReading(nowMilliseconds, timestamp, reading);
                });

            return m_token != 0;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to start a game controller step.")

        return false;
    }

    _Use_decl_annotations_
    void ControllerStepRunner::Update(BlockSettings const& settings) noexcept
    {
        std::lock_guard lock{ m_lock };

        m_button = settings.ControllerButton;
        m_axis = settings.ControllerAxis;
    }

    void ControllerStepRunner::Stop() noexcept
    {
        try
        {
            if (m_token != 0)
            {
                if (m_readsDevice)
                {
                    HidHub::Current().StopListening(m_token);
                }
                else
                {
                    ControllerHub::Current().StopListening(m_token);
                }

                m_token = 0;
            }

            std::lock_guard lock{ m_lock };

            auto const timestamp = internal::GetCurrentMidiTimestamp();

            ControllerSink const sink = [this, timestamp](uint32_t const* words, uint8_t count)
                {
                    m_send(timestamp, words, count);
                };

            if (m_kind == BlockKind::ControllerButton)
            {
                m_buttonMachine.Release(sink);
            }
            else
            {
                m_axisMachine.Release(m_axis, sink);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to stop a game controller step.")
    }

    _Use_decl_annotations_
    void ControllerStepRunner::OnReading(uint64_t nowMilliseconds, uint64_t timestamp, ControllerReading const& reading) noexcept
    {
        try
        {
            std::lock_guard lock{ m_lock };

            ControllerSink const sink = [this, timestamp](uint32_t const* words, uint8_t count)
                {
                    m_send(timestamp, words, count);
                };

            if (m_kind == BlockKind::ControllerButton)
            {
                m_buttonMachine.Update(m_button, nowMilliseconds, ControlIsDown(reading, m_button.Control), sink);
            }
            else
            {
                m_axisMachine.Update(m_axis, nowMilliseconds, reading.Connected, ControlValue(reading, m_axis.Control), sink);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to handle a game controller reading.")
    }

    _Use_decl_annotations_
    void ControllerStepRunner::OnHidReading(uint64_t nowMilliseconds, uint64_t timestamp, HidReading const& reading) noexcept
    {
        try
        {
            std::lock_guard lock{ m_lock };

            ControllerSink const sink = [this, timestamp](uint32_t const* words, uint8_t count)
                {
                    m_send(timestamp, words, count);
                };

            if (m_kind == BlockKind::ControllerButton)
            {
                m_buttonMachine.Update(m_button, nowMilliseconds, HidControlIsDown(reading, m_button.DeviceControl), sink);
            }
            else
            {
                m_axisMachine.Update(m_axis, nowMilliseconds, reading.Connected, HidControlValue(reading, m_axis.DeviceControl), sink);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to handle a game controller reading.")
    }
}
