// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The framing a device link uses inside its encrypted connection, and the small readers and
// writers for what goes in a frame. Everything here comes from another PC, so every length is
// checked against a cap before anything is allocated, and every read has an end.
//
// A frame is a 4-byte little-endian payload length, a 2-byte little-endian type, then the
// payload. Pch-free, so the unit tests compile it unchanged.

#include <sal.h>

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace midiapp
{
    constexpr size_t DeviceLinkFrameHeaderBytes = 6;

    // No frame is ever larger than this, whatever its type allows.
    constexpr uint32_t MaximumDeviceLinkFrameBytes = 1024 * 1024;

    struct DeviceLinkFrame
    {
        uint16_t Type{ 0 };
        std::vector<uint8_t> Payload{};
    };

    enum class DeviceLinkFrameStatus
    {
        Read,

        // The other side closed the connection cleanly before a frame started.
        Closed,

        // The connection ended, timed out or failed partway through a frame.
        Truncated,

        // Longer than its type allows.
        TooLarge,

        // A type that isn't expected at this point.
        UnexpectedType,
    };

    // Reads exactly `size` bytes, or returns false. `closedCleanly` is set when nothing at all
    // was read because the other side closed.
    using DeviceLinkReadExact = std::function<bool(uint8_t* destination, size_t size, bool& closedCleanly)>;

    // The largest payload a type may carry here, or 0 when the type isn't expected.
    using DeviceLinkFrameCap = std::function<uint32_t(uint16_t type)>;

    // Empty when the payload is larger than any frame may be.
    std::vector<uint8_t> EncodeDeviceLinkFrame(
        _In_ uint16_t type,
        _In_ std::span<uint8_t const> payload) noexcept;

    DeviceLinkFrameStatus ReadDeviceLinkFrame(
        _In_ DeviceLinkReadExact const& readExact,
        _In_ DeviceLinkFrameCap const& capFor,
        _Out_ DeviceLinkFrame& frame) noexcept;

    // Little-endian values and length-prefixed fields, for building a payload.
    class DeviceLinkWriter
    {
    public:
        void U8(_In_ uint8_t value);
        void U16(_In_ uint16_t value);
        void U32(_In_ uint32_t value);
        void U64(_In_ uint64_t value);
        void Raw(_In_ std::span<uint8_t const> bytes);

        // A 4-byte length, then the bytes.
        void Bytes(_In_ std::span<uint8_t const> bytes);

        // UTF-8 with a 4-byte length.
        void Text(_In_ std::wstring_view text);

        std::vector<uint8_t> const& Data() const noexcept { return m_data; }
        std::vector<uint8_t> Take() noexcept { return std::move(m_data); }

    private:
        std::vector<uint8_t> m_data{};
    };

    // The other half. Every read fails once the payload runs out or a length is over its cap,
    // and stays failed, so a caller can read every field and check Ok() once at the end.
    class DeviceLinkReader
    {
    public:
        explicit DeviceLinkReader(_In_ std::span<uint8_t const> payload) noexcept : m_data(payload) {}

        uint8_t U8() noexcept;
        uint16_t U16() noexcept;
        uint32_t U32() noexcept;
        uint64_t U64() noexcept;
        bool Raw(_Out_writes_bytes_(size) uint8_t* destination, _In_ size_t size) noexcept;

        std::vector<uint8_t> Bytes(_In_ size_t maximumBytes) noexcept;

        // Also fails on UTF-8 that isn't valid, and on more than `maximumCharacters` characters.
        std::wstring Text(_In_ size_t maximumCharacters) noexcept;

        // Every field was there and nothing is left over.
        bool Ok() const noexcept { return !m_failed && m_offset == m_data.size(); }
        bool Failed() const noexcept { return m_failed; }

    private:
        bool Take(_In_ size_t size, _Out_ std::span<uint8_t const>& taken) noexcept;

        std::span<uint8_t const> m_data{};
        size_t m_offset{ 0 };
        bool m_failed{ false };
    };

    std::string DeviceLinkUtf8(_In_ std::wstring_view text);

    // Empty for UTF-8 that isn't valid.
    std::wstring DeviceLinkFromUtf8(_In_ std::span<uint8_t const> utf8, _Out_ bool& valid);
}
