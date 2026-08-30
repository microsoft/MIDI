// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "SysExRow.g.h"

namespace winrt::midisysextool::implementation
{
    struct SysExRow : SysExRowT<SysExRow>
    {
        SysExRow() = default;

        SysExRow(
            winrt::hstring const& messageNumber,
            winrt::hstring const& words,
            uint8_t wordKind,
            winrt::hstring const& bytePrefix,
            winrt::hstring const& byteData,
            winrt::hstring const& byteSuffix,
            bool isOutOfSequence) :
            m_messageNumber(messageNumber),
            m_words(words),
            m_wordKind(wordKind),
            m_bytePrefix(bytePrefix),
            m_byteData(byteData),
            m_byteSuffix(byteSuffix),
            m_isOutOfSequence(isOutOfSequence)
        {
        }

        winrt::hstring MessageNumber() const noexcept { return m_messageNumber; }
        winrt::hstring Words() const noexcept { return m_words; }
        uint8_t WordKind() const noexcept { return m_wordKind; }
        winrt::hstring BytePrefix() const noexcept { return m_bytePrefix; }
        winrt::hstring ByteData() const noexcept { return m_byteData; }
        winrt::hstring ByteSuffix() const noexcept { return m_byteSuffix; }
        bool IsOutOfSequence() const noexcept { return m_isOutOfSequence; }

    private:
        winrt::hstring m_messageNumber{};
        winrt::hstring m_words{};
        uint8_t m_wordKind{ 0 };
        winrt::hstring m_bytePrefix{};
        winrt::hstring m_byteData{};
        winrt::hstring m_byteSuffix{};
        bool m_isOutOfSequence{ false };
    };
}

namespace winrt::midisysextool::factory_implementation
{
    struct SysExRow : SysExRowT<SysExRow, implementation::SysExRow>
    {
    };
}
