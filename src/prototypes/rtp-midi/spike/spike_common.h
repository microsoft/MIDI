// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. Helpers shared by the RTP-MIDI spike's commands.
// ============================================================================

#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include "rtpmidi_session.h"

namespace Spike
{
    inline std::mutex& ConsoleLock()
    {
        static std::mutex lock;
        return lock;
    }

    inline void Print(char const* format, ...)
    {
        char buffer[2048]{};

        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        auto lock = std::scoped_lock{ ConsoleLock() };
        fputs(buffer, stdout);
        fputc('\n', stdout);
        fflush(stdout);
    }

    inline std::string ToUtf8(std::wstring const& text)
    {
        if (text.empty()) return {};
        auto const size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    inline std::wstring ToWide(std::string const& text)
    {
        if (text.empty()) return {};
        auto const size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring result(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
        return result;
    }

    inline std::string Hex(uint8_t const* bytes, size_t count, size_t limit = 48)
    {
        std::string text;
        char part[4]{};

        for (size_t i = 0; i < count && i < limit; i++)
        {
            snprintf(part, sizeof(part), "%02X", bytes[i]);
            if (i > 0) text += ' ';
            text += part;
        }

        if (count > limit) text += " ... (" + std::to_string(count) + " bytes)";

        return text;
    }

    inline std::string Hex(std::vector<uint8_t> const& bytes, size_t limit = 48) { return Hex(bytes.data(), bytes.size(), limit); }

    inline std::string DescribeMidi(std::vector<uint8_t> const& bytes)
    {
        if (bytes.empty()) return "(empty)";

        auto const status = bytes[0];
        char text[96]{};

        if (status >= 0x80 && status < 0xF0 && bytes.size() >= 2)
        {
            int const channel = (status & 0x0F) + 1;
            int const d1 = bytes[1];
            int const d2 = bytes.size() > 2 ? bytes[2] : 0;

            switch (status & 0xF0)
            {
            case 0x80: snprintf(text, sizeof(text), "Note Off ch%d note %d vel %d", channel, d1, d2); break;
            case 0x90: snprintf(text, sizeof(text), "Note On ch%d note %d vel %d", channel, d1, d2); break;
            case 0xA0: snprintf(text, sizeof(text), "Poly Pressure ch%d note %d %d", channel, d1, d2); break;
            case 0xB0: snprintf(text, sizeof(text), "Control Change ch%d cc %d = %d", channel, d1, d2); break;
            case 0xC0: snprintf(text, sizeof(text), "Program Change ch%d %d", channel, d1); break;
            case 0xD0: snprintf(text, sizeof(text), "Channel Pressure ch%d %d", channel, d1); break;
            case 0xE0: snprintf(text, sizeof(text), "Pitch Bend ch%d %d", channel, (d2 << 7) | d1); break;
            default: break;
            }

            return text;
        }

        if (status == 0xF0 || (status < 0x80 && bytes.back() == 0xF7)) return "SysEx " + std::to_string(bytes.size()) + " bytes";

        switch (status)
        {
        case 0xF8: return "Clock";
        case 0xFA: return "Start";
        case 0xFB: return "Continue";
        case 0xFC: return "Stop";
        case 0xFE: return "Active Sensing";
        case 0xFF: return "Reset";
        case 0xF7: return "SysEx end";
        default: return "System";
        }
    }

    // The AppleMIDI session clock: 100 microsecond ticks, from QueryPerformanceCounter, which is
    // also the time base of Windows MIDI Services timestamps.
    class SessionClock
    {
    public:
        SessionClock()
        {
            LARGE_INTEGER frequency{};
            QueryPerformanceFrequency(&frequency);
            m_frequency = static_cast<uint64_t>(frequency.QuadPart);

            LARGE_INTEGER counter{};
            QueryPerformanceCounter(&counter);
            m_origin = static_cast<uint64_t>(counter.QuadPart);
        }

        uint64_t Now() const
        {
            LARGE_INTEGER counter{};
            QueryPerformanceCounter(&counter);
            auto const ticks = static_cast<uint64_t>(counter.QuadPart);

            return (ticks / m_frequency) * RtpMidi::SessionClockTicksPerSecond +
                ((ticks % m_frequency) * RtpMidi::SessionClockTicksPerSecond) / m_frequency;
        }

        double Milliseconds(uint64_t sessionTicks) const
        {
            return static_cast<double>(sessionTicks) / 10.0;
        }

        uint64_t OriginTicks() const
        {
            return (m_origin / m_frequency) * RtpMidi::SessionClockTicksPerSecond +
                ((m_origin % m_frequency) * RtpMidi::SessionClockTicksPerSecond) / m_frequency;
        }

    private:
        uint64_t m_frequency{ 1 };
        uint64_t m_origin{ 0 };
    };

    inline uint64_t SecureRandom64()
    {
        uint64_t value{ 0 };

        if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&value), sizeof(value), BCRYPT_USE_SYSTEM_PREFERRED_RNG)) || value == 0)
        {
            value = static_cast<uint64_t>(GetTickCount64()) * 0x9E3779B97F4A7C15ull;
        }

        return value;
    }

    inline std::atomic<bool>& StopRequested()
    {
        static std::atomic<bool> stop{ false };
        return stop;
    }

    inline BOOL WINAPI ConsoleControlHandler(DWORD type)
    {
        if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT)
        {
            StopRequested() = true;
            return TRUE;
        }

        return FALSE;
    }
}
