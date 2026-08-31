// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Messages.MidiBytestreamToUmpMessageConverterState.g.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Messages::implementation
{
    struct MidiBytestreamToUmpMessageConverterState : MidiBytestreamToUmpMessageConverterStateT<MidiBytestreamToUmpMessageConverterState>
    {
        MidiBytestreamToUmpMessageConverterState() = default;

        winrt::hstring Tag() noexcept { return m_tag; }
        void Tag(_In_ winrt::hstring const& value) noexcept { m_tag = value; }

        std::shared_ptr<bytestreamToUMP> InternalGetConverter() noexcept;

    private:
        winrt::hstring m_tag{};

        std::shared_ptr<bytestreamToUMP> m_converter{ nullptr };
    };
}
namespace winrt::Windows::Devices::Midi2::Utilities::Messages::factory_implementation
{
    struct MidiBytestreamToUmpMessageConverterState : MidiBytestreamToUmpMessageConverterStateT<MidiBytestreamToUmpMessageConverterState, implementation::MidiBytestreamToUmpMessageConverterState>
    {
    };
}
