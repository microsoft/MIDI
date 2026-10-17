// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The Windows kernel events the DPC latency check reads, and how to take them apart. These are
// the classic kernel events that Windows Performance Recorder and Analyzer use. Their layouts are
// documented on Microsoft Learn under "Event Tracing for Windows, MOF classes", and each decoder
// here checks the size it's given, because a newer Windows may add fields at the end.

#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

namespace miditroubleshooter::latency::kernel
{
    // {ce1dbfb4-137e-4da6-87b0-3f59aa102cbc}
    inline constexpr GUID PerfInfoGuid{ 0xce1dbfb4, 0x137e, 0x4da6, { 0x87, 0xb0, 0x3f, 0x59, 0xaa, 0x10, 0x2c, 0xbc } };

    // {3d6fa8d1-fe05-11d0-9dda-00c04fd7ba7c}
    inline constexpr GUID ThreadGuid{ 0x3d6fa8d1, 0xfe05, 0x11d0, { 0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c } };

    // {3d6fa8d0-fe05-11d0-9dda-00c04fd7ba7c}
    inline constexpr GUID ProcessGuid{ 0x3d6fa8d0, 0xfe05, 0x11d0, { 0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c } };

    // {2cb15d1d-5fc1-11d2-abe1-00a0c911f518}
    inline constexpr GUID ImageGuid{ 0x2cb15d1d, 0x5fc1, 0x11d2, { 0xab, 0xe1, 0x00, 0xa0, 0xc9, 0x11, 0xf5, 0x18 } };

    // {3d6fa8d3-fe05-11d0-9dda-00c04fd7ba7c}
    inline constexpr GUID PageFaultGuid{ 0x3d6fa8d3, 0xfe05, 0x11d0, { 0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c } };

    // {6a399ae0-4bc6-4de9-870b-3657f8947e7e}, written into a real-time session when it drops events
    inline constexpr GUID LostEventGuid{ 0x6a399ae0, 0x4bc6, 0x4de9, { 0x87, 0x0b, 0x36, 0x57, 0xf8, 0x94, 0x7e, 0x7e } };

    namespace opcode
    {
        inline constexpr uint8_t SampledProfile{ 46 };
        inline constexpr uint8_t IsrMsi{ 50 };
        inline constexpr uint8_t ThreadedDpc{ 66 };
        inline constexpr uint8_t Isr{ 67 };
        inline constexpr uint8_t Dpc{ 68 };
        inline constexpr uint8_t TimerDpc{ 69 };

        // logged by the Windows driver framework when it calls a driver's own routine, inside
        // the framework's ISR or DPC
        inline constexpr uint8_t WdfIsr{ 96 };
        inline constexpr uint8_t WdfDpc{ 98 };

        inline constexpr uint8_t Start{ 1 };
        inline constexpr uint8_t End{ 2 };
        inline constexpr uint8_t RundownStart{ 3 };
        inline constexpr uint8_t RundownEnd{ 4 };
        inline constexpr uint8_t ImageLoad{ 10 };
        inline constexpr uint8_t ContextSwitch{ 36 };
        inline constexpr uint8_t ReadyThread{ 50 };
        inline constexpr uint8_t ThreadSetName{ 72 };
        inline constexpr uint8_t HardFault{ 32 };

        inline constexpr uint8_t LostEvent{ 32 };
        inline constexpr uint8_t LostBuffer{ 33 };
    }

    inline bool SameGuid(_In_ GUID const& left, _In_ GUID const& right) noexcept
    {
        return std::memcmp(&left, &right, sizeof(GUID)) == 0;
    }

    inline uint32_t PointerSize(_In_ EVENT_RECORD const& record) noexcept
    {
        return (record.EventHeader.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER) != 0 ? 4u : 8u;
    }

    // Reads one value from the event's data, or returns false when the data is too short.
    class Reader
    {
    public:
        explicit Reader(_In_ EVENT_RECORD const& record) noexcept :
            m_data{ static_cast<uint8_t const*>(record.UserData) },
            m_length{ record.UserDataLength },
            m_pointerSize{ PointerSize(record) }
        {
        }

        uint32_t Length() const noexcept { return m_length; }

        template <typename T>
        bool Read(_In_ uint32_t const offset, _Out_ T& value) const noexcept
        {
            value = T{};

            if (m_data == nullptr || offset > m_length || m_length - offset < sizeof(T))
            {
                return false;
            }

            std::memcpy(&value, m_data + offset, sizeof(T));
            return true;
        }

        // a pointer-sized value, which is 4 bytes in a trace from a 32-bit Windows
        bool ReadPointer(_In_ uint32_t const offset, _Out_ uint64_t& value) const noexcept
        {
            value = 0;

            if (m_pointerSize == 4)
            {
                uint32_t narrow{ 0 };

                if (!Read(offset, narrow))
                {
                    return false;
                }

                value = narrow;
                return true;
            }

            return Read(offset, value);
        }

        // a null terminated UTF-16 string, cut off at the end of the data if it isn't terminated
        std::wstring ReadWideString(_In_ uint32_t const offset) const
        {
            if (m_data == nullptr || offset >= m_length)
            {
                return {};
            }

            auto const characters = (m_length - offset) / sizeof(wchar_t);
            std::wstring text{};
            text.reserve(characters);

            for (size_t index = 0; index < characters; ++index)
            {
                wchar_t character{};
                std::memcpy(&character, m_data + offset + index * sizeof(wchar_t), sizeof(wchar_t));

                if (character == L'\0')
                {
                    break;
                }

                text.push_back(character);
            }

            return text;
        }

        uint32_t PointerBytes() const noexcept { return m_pointerSize; }

    private:
        uint8_t const* m_data{ nullptr };
        uint32_t m_length{ 0 };
        uint32_t m_pointerSize{ 8 };
    };

    // DPC, threaded DPC, timer DPC: logged when the routine returns.
    struct DpcEvent
    {
        uint64_t InitialTime{ 0 };
        uint64_t Routine{ 0 };
    };

    inline std::optional<DpcEvent> DecodeDpc(_In_ EVENT_RECORD const& record) noexcept
    {
        Reader reader{ record };
        DpcEvent value{};

        if (!reader.Read(0, value.InitialTime) || !reader.ReadPointer(8, value.Routine))
        {
            return std::nullopt;
        }

        return value;
    }

    // ISR and ISR-MSI start the same way as a DPC.
    inline std::optional<DpcEvent> DecodeIsr(_In_ EVENT_RECORD const& record) noexcept
    {
        return DecodeDpc(record);
    }

    // The framework's events carry only the driver's routine.
    inline std::optional<uint64_t> DecodeFrameworkRoutine(_In_ EVENT_RECORD const& record) noexcept
    {
        Reader reader{ record };
        uint64_t routine{ 0 };

        if (!reader.ReadPointer(0, routine))
        {
            return std::nullopt;
        }

        return routine;
    }

    struct SampleEvent
    {
        uint64_t InstructionPointer{ 0 };
        uint32_t ThreadId{ 0 };
    };

    inline std::optional<SampleEvent> DecodeSample(_In_ EVENT_RECORD const& record) noexcept
    {
        Reader reader{ record };
        SampleEvent value{};

        if (!reader.ReadPointer(0, value.InstructionPointer) || !reader.Read(reader.PointerBytes(), value.ThreadId))
        {
            return std::nullopt;
        }

        return value;
    }

    // Only the first 24 bytes are documented. Windows 11 adds 4 more, and has no schema for them.
    struct ContextSwitchEvent
    {
        uint32_t NewThreadId{ 0 };
        uint32_t OldThreadId{ 0 };
        int8_t NewThreadPriority{ 0 };
        int8_t OldThreadPriority{ 0 };
        uint8_t PreviousCState{ 0 };
        int8_t OldThreadWaitReason{ 0 };
        int8_t OldThreadState{ 0 };
    };

    inline std::optional<ContextSwitchEvent> DecodeContextSwitch(_In_ EVENT_RECORD const& record) noexcept
    {
        Reader reader{ record };
        ContextSwitchEvent value{};

        if (reader.Length() < 24)
        {
            return std::nullopt;
        }

        reader.Read(0, value.NewThreadId);
        reader.Read(4, value.OldThreadId);
        reader.Read(8, value.NewThreadPriority);
        reader.Read(9, value.OldThreadPriority);
        reader.Read(10, value.PreviousCState);
        reader.Read(12, value.OldThreadWaitReason);
        reader.Read(14, value.OldThreadState);

        return value;
    }

    inline std::optional<uint32_t> DecodeReadyThread(_In_ EVENT_RECORD const& record) noexcept
    {
        Reader reader{ record };
        uint32_t threadId{ 0 };

        if (!reader.Read(0, threadId))
        {
            return std::nullopt;
        }

        return threadId;
    }

    // thread start, end and rundown
    struct ThreadEvent
    {
        uint32_t ProcessId{ 0 };
        uint32_t ThreadId{ 0 };
    };

    inline std::optional<ThreadEvent> DecodeThread(_In_ EVENT_RECORD const& record) noexcept
    {
        Reader reader{ record };
        ThreadEvent value{};

        if (!reader.Read(0, value.ProcessId) || !reader.Read(4, value.ThreadId))
        {
            return std::nullopt;
        }

        return value;
    }

    struct ThreadNameEvent
    {
        uint32_t ProcessId{ 0 };
        uint32_t ThreadId{ 0 };
        std::wstring Name{};
    };

    inline std::optional<ThreadNameEvent> DecodeThreadName(_In_ EVENT_RECORD const& record)
    {
        Reader reader{ record };
        ThreadNameEvent value{};

        if (!reader.Read(0, value.ProcessId) || !reader.Read(4, value.ThreadId))
        {
            return std::nullopt;
        }

        value.Name = reader.ReadWideString(8);
        return value;
    }

    // image load, unload and rundown
    struct ImageEvent
    {
        uint64_t Base{ 0 };
        uint64_t Size{ 0 };
        uint32_t ProcessId{ 0 };
        uint32_t Checksum{ 0 };
        uint32_t TimeDateStamp{ 0 };
        std::wstring FileName{};
    };

    inline std::optional<ImageEvent> DecodeImage(_In_ EVENT_RECORD const& record)
    {
        Reader reader{ record };
        auto const pointer = reader.PointerBytes();
        ImageEvent value{};

        // base, size, pid, checksum, time stamp, 4 bytes, default base, 16 bytes, then the name
        auto const nameOffset = 2 * pointer + 4 * 4 + pointer + 4 * 4;

        if (!reader.ReadPointer(0, value.Base) ||
            !reader.ReadPointer(pointer, value.Size) ||
            !reader.Read(2 * pointer, value.ProcessId) ||
            !reader.Read(2 * pointer + 4, value.Checksum) ||
            !reader.Read(2 * pointer + 8, value.TimeDateStamp) ||
            reader.Length() < nameOffset)
        {
            return std::nullopt;
        }

        value.FileName = reader.ReadWideString(nameOffset);
        return value;
    }

    struct HardFaultEvent
    {
        uint64_t InitialTime{ 0 };
        uint32_t ThreadId{ 0 };
    };

    inline std::optional<HardFaultEvent> DecodeHardFault(_In_ EVENT_RECORD const& record) noexcept
    {
        Reader reader{ record };
        auto const pointer = reader.PointerBytes();
        HardFaultEvent value{};

        // initial time, read offset, virtual address, file object, then the thread
        if (!reader.Read(0, value.InitialTime) || !reader.Read(16 + 2 * pointer, value.ThreadId))
        {
            return std::nullopt;
        }

        return value;
    }

    // True for an address in the kernel's half of the address space.
    inline bool IsKernelAddress(_In_ uint64_t const address, _In_ uint32_t const pointerSize) noexcept
    {
        return pointerSize == 8 ? (address >> 63) != 0 : (address >> 31) != 0;
    }
}
