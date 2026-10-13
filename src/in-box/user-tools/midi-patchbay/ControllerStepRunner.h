// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "ControllerSteps.h"
#include "ProcessingBlock.h"

namespace midipatchbay
{
    // A game controller step while its patch routes. Listens to ControllerHub and hands what the
    // step makes to send, on the hub's thread.
    class ControllerStepRunner
    {
    public:
        using Send = std::function<void(uint64_t timestamp, uint32_t const* words, uint32_t wordCount)>;

        ControllerStepRunner(
            _In_ BlockKind kind,
            _In_ std::wstring patchKey,
            _In_ BlockSettings const& settings,
            _In_ Send send);

        ~ControllerStepRunner();

        ControllerStepRunner(ControllerStepRunner const&) = delete;
        ControllerStepRunner& operator=(ControllerStepRunner const&) = delete;

        bool Start() noexcept;

        // Everything but the controller and the control changes on the fly.
        void Update(_In_ BlockSettings const& settings) noexcept;

        // Stops listening, then lets go of anything still on: a note still sounding, a value
        // still held, a stick left bent.
        void Stop() noexcept;

    private:
        void OnReading(_In_ uint64_t nowMilliseconds, _In_ uint64_t timestamp, _In_ ControllerReading const& reading) noexcept;
        void OnHidReading(_In_ uint64_t nowMilliseconds, _In_ uint64_t timestamp, _In_ HidReading const& reading) noexcept;

        BlockKind const m_kind;
        std::wstring const m_patchKey;
        Send const m_send;

        std::mutex m_lock{};
        ControllerButtonSettings m_button{};
        ControllerAxisSettings m_axis{};
        ControllerButtonMachine m_buttonMachine;
        ControllerAxisMachine m_axisMachine{};

        uint64_t m_token{ 0 };
        bool m_readsDevice{ false };
    };

    // Where next and previous program left off in one patch, on one group and channel. Kept for
    // as long as the app runs, like a memory.
    ProgramPosition SharedProgramPosition(_In_ std::wstring const& patchKey, _In_ uint8_t group, _In_ uint8_t channel);
}
