// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "OscTranslate.h"
#include "OscAddress.h"
#include "OscRules.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <format>

namespace midipatchbay::osc
{
    namespace
    {
        // Every kind of message a row can carry, whatever its format calls it.
        enum class Kind : uint8_t
        {
            Note,
            NoteOff,
            PolyPressure,
            ControlChange,
            Program,
            ChannelPressure,
            PitchBend,
            SysEx,
            Rpn,
            Nrpn,
            Clock,
            Start,
            Continue,
            Stop,
            ActiveSensing,
            Reset,
            SongPosition,
            SongSelect,
            TuneRequest,
            TimeCode,
        };

        struct KindName
        {
            Kind Value;
            std::string_view Name;
        };

        // Paths: <root>/<channel>/<name>/<number> and <root>/<channel>/<name>. The names are
        // REAPER's. The system names are ours, under the same pattern.
        constexpr std::array<KindName, 4> PathNumberedKinds
        { {
            { Kind::Note, "note" },
            { Kind::NoteOff, "noteoff" },
            { Kind::PolyPressure, "polyaftertouch" },
            { Kind::ControlChange, "cc" },
        } };

        constexpr std::array<KindName, 3> PathChannelKinds
        { {
            { Kind::Program, "program" },
            { Kind::ChannelPressure, "channelpressure" },
            { Kind::PitchBend, "pitch" },
        } };

        constexpr std::array<KindName, 11> SystemKinds
        { {
            { Kind::SysEx, "sysex" },
            { Kind::Clock, "clock" },
            { Kind::Start, "start" },
            { Kind::Continue, "continue" },
            { Kind::Stop, "stop" },
            { Kind::ActiveSensing, "activesensing" },
            { Kind::Reset, "reset" },
            { Kind::SongPosition, "songposition" },
            { Kind::SongSelect, "songselect" },
            { Kind::TuneRequest, "tunerequest" },
            { Kind::TimeCode, "timecode" },
        } };

        // Values in arguments, with the channel first. The names Open Stage Control uses.
        constexpr std::array<KindName, 9> OpenStageControlKinds
        { {
            { Kind::Note, "note" },
            { Kind::NoteOff, "note_off" },
            { Kind::ControlChange, "control" },
            { Kind::Program, "program" },
            { Kind::PitchBend, "pitch" },
            { Kind::ChannelPressure, "channel_pressure" },
            { Kind::PolyPressure, "key_pressure" },
            { Kind::Rpn, "rpn" },
            { Kind::Nrpn, "nrpn" },
        } };

        // Values in arguments, with no channel. The names OSC_MIDI_OSC uses.
        constexpr std::array<KindName, 7> OscMidiOscKinds
        { {
            { Kind::Note, "noteOn" },
            { Kind::NoteOff, "noteOff" },
            { Kind::ControlChange, "cc" },
            { Kind::ChannelPressure, "Aftertouch" },
            { Kind::PolyPressure, "PolyAftertouch" },
            { Kind::PitchBend, "Pitchbend" },
            { Kind::Program, "ProgramChange" },
        } };

        template <size_t N>
        std::optional<Kind> FindKind(_In_ std::array<KindName, N> const& names, _In_ std::string_view name) noexcept
        {
            for (auto const& entry : names)
            {
                if (entry.Name == name)
                {
                    return entry.Value;
                }
            }

            return std::nullopt;
        }

        template <size_t N>
        std::string_view NameOf(_In_ std::array<KindName, N> const& names, _In_ Kind kind) noexcept
        {
            for (auto const& entry : names)
            {
                if (entry.Value == kind)
                {
                    return entry.Name;
                }
            }

            return {};
        }

        // A decimal number with no sign and no leading zeros past the first digit.
        std::optional<uint32_t> ParseDecimal(_In_ std::string_view text) noexcept
        {
            if (text.empty() || text.size() > 5 || (text.size() > 1 && text.front() == '0'))
            {
                return std::nullopt;
            }

            uint32_t value{ 0 };

            for (auto const c : text)
            {
                if (c < '0' || c > '9')
                {
                    return std::nullopt;
                }

                value = value * 10 + static_cast<uint32_t>(c - '0');
            }

            return value;
        }

        std::optional<uint8_t> ParseChannel(_In_ OscRow const& row, _In_ std::string_view text) noexcept
        {
            auto const value = ParseDecimal(text);

            if (!value.has_value())
            {
                return std::nullopt;
            }

            auto const first = row.ChannelsFromZero ? 0u : 1u;

            if (*value < first || *value > first + 15)
            {
                return std::nullopt;
            }

            return static_cast<uint8_t>(*value - first);
        }

        bool IsNumberType(_In_ char type) noexcept
        {
            return type == type::Int32 || type == type::Int64 || type == type::Float32 || type == type::Double ||
                type == type::True || type == type::False;
        }

        bool IsFloatType(_In_ char type) noexcept
        {
            return type == type::Float32 || type == type::Double;
        }

        // A whole number taken as it is: a channel, a note or a controller. Floats are rounded.
        std::optional<int64_t> ReadWhole(_In_ Argument const& argument) noexcept
        {
            if (IsFloatType(argument.Type))
            {
                if (!std::isfinite(argument.Real) || std::fabs(argument.Real) > 1.0e9)
                {
                    return std::nullopt;
                }

                return static_cast<int64_t>(std::llround(argument.Real));
            }

            if (argument.Type == type::Int32 || argument.Type == type::Int64 ||
                argument.Type == type::True || argument.Type == type::False)
            {
                return argument.Integer;
            }

            return std::nullopt;
        }

        // A value up to highest. A float on a "0 to 1" row is a fraction of it; T is the most and
        // F the least.
        std::optional<uint32_t> ReadValue(_In_ OscRow const& row, _In_ Argument const& argument, _In_ uint32_t highest) noexcept
        {
            if (argument.Type == type::True)
            {
                return highest;
            }

            if (argument.Type == type::False)
            {
                return 0u;
            }

            if (IsFloatType(argument.Type))
            {
                if (!std::isfinite(argument.Real))
                {
                    return std::nullopt;
                }

                auto const scaled = row.Values == OscValueKind::UnitFloats ? argument.Real * highest : argument.Real;

                return static_cast<uint32_t>(std::clamp(std::llround(std::clamp(scaled, -1.0e9, 1.0e9)), 0ll, static_cast<long long>(highest)));
            }

            if (argument.Type == type::Int32 || argument.Type == type::Int64)
            {
                return static_cast<uint32_t>(std::clamp(argument.Integer, int64_t{ 0 }, static_cast<int64_t>(highest)));
            }

            return std::nullopt;
        }

        std::optional<uint16_t> ReadPitchBend(_In_ OscRow const& row, _In_ Argument const& argument) noexcept
        {
            if (argument.Type == type::True)
            {
                return uint16_t{ 16383 };
            }

            if (argument.Type == type::False)
            {
                return uint16_t{ 0 };
            }

            auto const offset = row.PitchBend == OscPitchBendRange::Signed ? 8192.0 : 0.0;

            if (IsFloatType(argument.Type))
            {
                if (!std::isfinite(argument.Real))
                {
                    return std::nullopt;
                }

                if (row.Values == OscValueKind::UnitFloats)
                {
                    return PitchBendFromUnit(argument.Real);
                }

                return static_cast<uint16_t>(std::clamp(std::llround(std::clamp(argument.Real + offset, -1.0e9, 1.0e9)), 0ll, 16383ll));
            }

            if (argument.Type == type::Int32 || argument.Type == type::Int64)
            {
                return static_cast<uint16_t>(std::clamp(argument.Integer + static_cast<int64_t>(offset), int64_t{ 0 }, int64_t{ 16383 }));
            }

            return std::nullopt;
        }

        // Where one received message's MIDI goes: the receiver, counted and capped.
        struct Output
        {
            UmpReceiver& Receiver;
            uint32_t Messages{ 0 };
            bool Capped{ false };

            bool Room() const noexcept
            {
                return Messages < MaximumMessagesFromOneOscMessage;
            }

            void Midi1(_In_ uint8_t group, _In_ uint8_t status, _In_ uint8_t data1, _In_ uint8_t data2) noexcept
            {
                if (!Room())
                {
                    Capped = true;
                    return;
                }

                // Channel voice messages are UMP type 2, everything else type 1.
                uint32_t const type = status < 0xF0 ? 0x2u : 0x1u;

                uint32_t const word = (type << 28) | (static_cast<uint32_t>(group & 0x0F) << 24) |
                    (static_cast<uint32_t>(status) << 16) | (static_cast<uint32_t>(data1 & 0x7F) << 8) | (data2 & 0x7Fu);

                Receiver.OnUmp(&word, 1);
                Messages++;
            }

            // The bytes between F0 and F7.
            void SysEx(_In_ uint8_t group, _In_reads_(count) uint8_t const* bytes, _In_ size_t count) noexcept
            {
                if (!Room())
                {
                    Capped = true;
                    return;
                }

                size_t position{ 0 };

                do
                {
                    auto const chunk = (std::min)(count - position, size_t{ 6 });
                    auto const first = position == 0;
                    auto const last = position + chunk == count;

                    uint32_t const status = first && last ? 0u : first ? 1u : last ? 3u : 2u;

                    std::array<uint8_t, 6> data{};
                    std::copy_n(bytes + position, chunk, data.begin());

                    uint32_t const words[2]
                    {
                        (0x3u << 28) | (static_cast<uint32_t>(group & 0x0F) << 24) | (status << 20) |
                            (static_cast<uint32_t>(chunk) << 16) | (static_cast<uint32_t>(data[0]) << 8) | data[1],
                        (static_cast<uint32_t>(data[2]) << 24) | (static_cast<uint32_t>(data[3]) << 16) |
                            (static_cast<uint32_t>(data[4]) << 8) | data[5],
                    };

                    Receiver.OnUmp(words, 2);
                    position += chunk;
                } while (position < count);

                Messages++;
            }
        };

        int HexDigit(_In_ char c) noexcept
        {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        }

        // SysEx from a blob, a hex string ("f0 7e ... f7" or "f07e...f7") or a list of numbers.
        // F0 and F7 are optional. The data bytes are copied into buffer.
        std::optional<size_t> ReadSysEx(
            _In_ MessageView const& message,
            _In_ size_t firstArgument,
            _Out_writes_to_(capacity, return) uint8_t* buffer,
            _In_ size_t capacity) noexcept
        {
            size_t count{ 0 };

            auto const add = [&](uint32_t value) noexcept
                {
                    if (value > 0xFF || count >= capacity)
                    {
                        return false;
                    }

                    buffer[count++] = static_cast<uint8_t>(value);
                    return true;
                };

            if (firstArgument >= message.ArgumentCount)
            {
                return std::nullopt;
            }

            auto const& first = message.Arguments[firstArgument];

            if (first.Type == type::Blob)
            {
                for (auto const b : first.Bytes)
                {
                    if (!add(b))
                    {
                        return std::nullopt;
                    }
                }
            }
            else if (first.Type == type::String || first.Type == type::Symbol)
            {
                int pending{ -1 };

                for (auto const c : first.Text)
                {
                    if (c == ' ' || c == ',')
                    {
                        if (pending >= 0 && !add(static_cast<uint32_t>(pending)))
                        {
                            return std::nullopt;
                        }

                        pending = -1;
                        continue;
                    }

                    auto const digit = HexDigit(c);

                    if (digit < 0)
                    {
                        return std::nullopt;
                    }

                    if (pending < 0)
                    {
                        pending = digit;
                    }
                    else
                    {
                        if (!add(static_cast<uint32_t>(pending * 16 + digit)))
                        {
                            return std::nullopt;
                        }

                        pending = -1;
                    }
                }

                if (pending >= 0 && !add(static_cast<uint32_t>(pending)))
                {
                    return std::nullopt;
                }
            }
            else
            {
                for (size_t i = firstArgument; i < message.ArgumentCount; i++)
                {
                    auto const value = ReadWhole(message.Arguments[i]);

                    if (!value.has_value() || *value < 0 || !add(static_cast<uint32_t>(*value)))
                    {
                        return std::nullopt;
                    }
                }
            }

            // Without F0 and F7, every byte is data.
            size_t start{ 0 };
            auto end = count;

            if (end > 0 && buffer[0] == 0xF0)
            {
                start = 1;
            }

            if (end > start && buffer[end - 1] == 0xF7)
            {
                end--;
            }

            for (auto i = start; i < end; i++)
            {
                if (buffer[i] >= 0x80)
                {
                    return std::nullopt;
                }
            }

            if (start > 0)
            {
                std::copy(buffer + start, buffer + end, buffer);
            }

            return end - start;
        }

        // Makes the MIDI for one message whose kind, channel and number its address gave, from
        // the arguments starting at the given one. False when the arguments don't fit.
        bool Emit(
            _In_ OscRow const& row,
            _In_ Kind kind,
            _In_ uint8_t channel,
            _In_ uint32_t number,
            _In_ MessageView const& message,
            _In_ size_t firstArgument,
            _Inout_ Output& output) noexcept
        {
            auto const group = row.Group;
            auto const count = message.ArgumentCount > firstArgument ? message.ArgumentCount - firstArgument : 0;
            auto const* const arguments = message.Arguments.data() + firstArgument;

            auto const value7 = [&](size_t index) noexcept -> std::optional<uint8_t>
                {
                    if (index >= count)
                    {
                        return std::nullopt;
                    }

                    auto const value = ReadValue(row, arguments[index], 127);
                    return value.has_value() ? std::optional<uint8_t>{ static_cast<uint8_t>(*value) } : std::nullopt;
                };

            if (number > 127 && kind != Kind::SongPosition)
            {
                return false;
            }

            auto const n = static_cast<uint8_t>(number);

            switch (kind)
            {
            case Kind::Note:
            {
                auto const velocity = value7(0);

                if (!velocity.has_value())
                {
                    return false;
                }

                // Velocity 0 is a note off, REAPER's rule and MIDI 1.0's.
                output.Midi1(group, static_cast<uint8_t>((*velocity == 0 ? 0x80 : 0x90) | channel), n, *velocity);
                return true;
            }

            case Kind::NoteOff:
            {
                auto const velocity = count > 0 ? value7(0) : std::optional<uint8_t>{ uint8_t{ 0 } };

                if (!velocity.has_value())
                {
                    return false;
                }

                output.Midi1(group, static_cast<uint8_t>(0x80 | channel), n, *velocity);
                return true;
            }

            case Kind::PolyPressure:
            case Kind::ControlChange:
            {
                auto const value = value7(0);

                if (!value.has_value())
                {
                    return false;
                }

                output.Midi1(group, static_cast<uint8_t>((kind == Kind::PolyPressure ? 0xA0 : 0xB0) | channel), n, *value);
                return true;
            }

            case Kind::Program:
            case Kind::ChannelPressure:
            {
                auto const value = value7(0);

                if (!value.has_value())
                {
                    return false;
                }

                output.Midi1(group, static_cast<uint8_t>((kind == Kind::Program ? 0xC0 : 0xD0) | channel), *value, 0);
                return true;
            }

            case Kind::PitchBend:
            {
                if (count == 0)
                {
                    return false;
                }

                auto const bend = ReadPitchBend(row, arguments[0]);

                if (!bend.has_value())
                {
                    return false;
                }

                output.Midi1(group, static_cast<uint8_t>(0xE0 | channel), static_cast<uint8_t>(*bend & 0x7F), static_cast<uint8_t>(*bend >> 7));
                return true;
            }

            case Kind::Rpn:
            case Kind::Nrpn:
            {
                if (count < 2)
                {
                    return false;
                }

                auto const parameter = ReadWhole(arguments[0]);
                auto const value = ReadWhole(arguments[1]);

                if (!parameter.has_value() || !value.has_value() ||
                    *parameter < 0 || *parameter > 16383 || *value < 0 || *value > 16383)
                {
                    return false;
                }

                auto const status = static_cast<uint8_t>(0xB0 | channel);
                auto const rpn = kind == Kind::Rpn;

                output.Midi1(group, status, rpn ? 101 : 99, static_cast<uint8_t>(*parameter >> 7));
                output.Midi1(group, status, rpn ? 100 : 98, static_cast<uint8_t>(*parameter & 0x7F));
                output.Midi1(group, status, 6, static_cast<uint8_t>(*value >> 7));
                output.Midi1(group, status, 38, static_cast<uint8_t>(*value & 0x7F));
                return true;
            }

            case Kind::SysEx:
            {
                std::array<uint8_t, MaximumSysExBytes> buffer;
                auto const length = ReadSysEx(message, firstArgument, buffer.data(), buffer.size());

                if (!length.has_value())
                {
                    return false;
                }

                output.SysEx(group, buffer.data(), *length);
                return true;
            }

            case Kind::Clock: output.Midi1(group, 0xF8, 0, 0); return true;
            case Kind::Start: output.Midi1(group, 0xFA, 0, 0); return true;
            case Kind::Continue: output.Midi1(group, 0xFB, 0, 0); return true;
            case Kind::Stop: output.Midi1(group, 0xFC, 0, 0); return true;
            case Kind::ActiveSensing: output.Midi1(group, 0xFE, 0, 0); return true;
            case Kind::Reset: output.Midi1(group, 0xFF, 0, 0); return true;
            case Kind::TuneRequest: output.Midi1(group, 0xF6, 0, 0); return true;

            case Kind::SongPosition:
            {
                if (count == 0)
                {
                    return false;
                }

                auto const position = ReadWhole(arguments[0]);

                if (!position.has_value() || *position < 0 || *position > 16383)
                {
                    return false;
                }

                output.Midi1(group, 0xF2, static_cast<uint8_t>(*position & 0x7F), static_cast<uint8_t>(*position >> 7));
                return true;
            }

            case Kind::SongSelect:
            case Kind::TimeCode:
            {
                if (count == 0)
                {
                    return false;
                }

                auto const value = ReadWhole(arguments[0]);

                if (!value.has_value() || *value < 0 || *value > 127)
                {
                    return false;
                }

                output.Midi1(group, kind == Kind::SongSelect ? 0xF3 : 0xF1, static_cast<uint8_t>(*value), 0);
                return true;
            }
            }

            return false;
        }

        enum class Match : uint8_t
        {
            // Not this row's address.
            No,

            // This row's address, and the MIDI was made.
            Done,

            // This row's address, but not arguments it can read.
            Unreadable,
        };

        // The part of the address under the root, starting with its slash. Nothing when the
        // address isn't under the root.
        std::optional<std::string_view> UnderRoot(_In_ OscRow const& row, _In_ std::string_view address) noexcept
        {
            if (address.size() <= row.Root.size() || address.substr(0, row.Root.size()) != row.Root || address[row.Root.size()] != '/')
            {
                return std::nullopt;
            }

            return address.substr(row.Root.size());
        }

        // Splits "/a/b/c" into its parts. False past four, or with an empty part.
        bool SplitRest(_In_ std::string_view rest, _Out_ std::array<std::string_view, 4>& parts, _Out_ size_t& count) noexcept
        {
            count = 0;
            parts = {};

            rest.remove_prefix(1);

            for (;;)
            {
                if (count == parts.size())
                {
                    return false;
                }

                auto const slash = rest.find('/');
                auto const part = rest.substr(0, slash);

                if (part.empty())
                {
                    return false;
                }

                parts[count++] = part;

                if (slash == std::string_view::npos)
                {
                    return true;
                }

                rest.remove_prefix(slash + 1);
            }
        }

        Match TranslatePaths(_In_ OscRow const& row, _In_ std::string_view address, _In_ MessageView const& message, _Inout_ Output& output) noexcept
        {
            auto const rest = UnderRoot(row, address);

            std::array<std::string_view, 4> parts{};
            size_t count{ 0 };

            if (!rest.has_value() || !SplitRest(*rest, parts, count))
            {
                return Match::No;
            }

            std::optional<Kind> kind{};
            uint8_t channel{ row.Channel };
            uint32_t number{ 0 };

            if (count == 1)
            {
                // /sysex, /clock... or REAPER's channel-less /program, /channelpressure, /pitch.
                kind = FindKind(SystemKinds, parts[0]);

                if (!kind.has_value())
                {
                    kind = FindKind(PathChannelKinds, parts[0]);
                }
            }
            else if (count == 2)
            {
                if (auto const parsed = ParseChannel(row, parts[0]); parsed.has_value())
                {
                    kind = FindKind(PathChannelKinds, parts[1]);
                    channel = *parsed;
                }
                else if (auto const numbered = FindKind(PathNumberedKinds, parts[0]); numbered.has_value())
                {
                    // REAPER's channel-less /note/60.
                    auto const parsedNumber = ParseDecimal(parts[1]);

                    if (parsedNumber.has_value())
                    {
                        kind = numbered;
                        number = *parsedNumber;
                    }
                }
            }
            else if (count == 3)
            {
                auto const parsed = ParseChannel(row, parts[0]);
                auto const numbered = FindKind(PathNumberedKinds, parts[1]);
                auto const parsedNumber = ParseDecimal(parts[2]);

                if (parsed.has_value() && numbered.has_value() && parsedNumber.has_value())
                {
                    kind = numbered;
                    channel = *parsed;
                    number = *parsedNumber;
                }
            }

            if (!kind.has_value())
            {
                return Match::No;
            }

            return Emit(row, *kind, channel, number, message, 0, output) ? Match::Done : Match::Unreadable;
        }

        Match TranslateMidiType(_In_ OscRow const& row, _In_ std::string_view address, _In_ MessageView const& message, _Inout_ Output& output) noexcept
        {
            if (auto const rest = UnderRoot(row, address); rest.has_value())
            {
                if (*rest != "/sysex")
                {
                    return Match::No;
                }

                return Emit(row, Kind::SysEx, 0, 0, message, 0, output) ? Match::Done : Match::Unreadable;
            }

            if (address != row.Root)
            {
                return Match::No;
            }

            // Several rows can share a root. Each takes the m arguments with its own port id.
            auto matched = false;
            auto unreadable = false;

            for (size_t i = 0; i < message.ArgumentCount; i++)
            {
                auto const& argument = message.Arguments[i];

                if (argument.Type != type::Midi)
                {
                    continue;
                }

                auto const packed = static_cast<uint32_t>(argument.Integer);
                auto const port = static_cast<uint8_t>(packed >> 24);
                auto const status = static_cast<uint8_t>(packed >> 16);
                auto const data1 = static_cast<uint8_t>(packed >> 8);
                auto const data2 = static_cast<uint8_t>(packed);

                if (port != row.Group)
                {
                    continue;
                }

                matched = true;

                // m can't carry SysEx, and a status byte has its top bit set.
                if (status < 0x80 || status == 0xF0 || status == 0xF7 || data1 >= 0x80 || data2 >= 0x80)
                {
                    unreadable = true;
                    continue;
                }

                output.Midi1(row.Group, status, data1, data2);
            }

            return !matched ? Match::No : unreadable ? Match::Unreadable : Match::Done;
        }

        Match TranslateArguments(_In_ OscRow const& row, _In_ std::string_view address, _In_ MessageView const& message, _Inout_ Output& output) noexcept
        {
            auto const rest = UnderRoot(row, address);

            if (!rest.has_value() || rest->find('/', 1) != std::string_view::npos)
            {
                return Match::No;
            }

            auto const name = rest->substr(1);
            auto const openStage = row.Names == OscArgumentNames::OpenStageControl;

            auto kind = openStage ? FindKind(OpenStageControlKinds, name) : FindKind(OscMidiOscKinds, name);

            if (!kind.has_value())
            {
                kind = FindKind(SystemKinds, name);

                if (!kind.has_value())
                {
                    return Match::No;
                }

                return Emit(row, *kind, 0, 0, message, 0, output) ? Match::Done : Match::Unreadable;
            }

            size_t next{ 0 };
            auto channel = row.Channel;

            if (openStage)
            {
                if (message.ArgumentCount == 0)
                {
                    return Match::Unreadable;
                }

                auto const value = ReadWhole(message.Arguments[0]);
                auto const first = row.ChannelsFromZero ? 0 : 1;

                if (!value.has_value() || *value < first || *value > first + 15)
                {
                    return Match::Unreadable;
                }

                channel = static_cast<uint8_t>(*value - first);
                next = 1;
            }

            uint32_t number{ 0 };

            if (*kind == Kind::Note || *kind == Kind::NoteOff || *kind == Kind::PolyPressure || *kind == Kind::ControlChange)
            {
                if (next >= message.ArgumentCount)
                {
                    return Match::Unreadable;
                }

                auto const value = ReadWhole(message.Arguments[next]);

                if (!value.has_value() || *value < 0 || *value > 127)
                {
                    return Match::Unreadable;
                }

                number = static_cast<uint32_t>(*value);
                next++;
            }

            return Emit(row, *kind, channel, number, message, next, output) ? Match::Done : Match::Unreadable;
        }

        // How far up its range a rule's argument is, from 0 to 1. True is the top and false the
        // bottom, whichever way round the range is.
        std::optional<double> RuleUnit(_In_ OscRule const& rule, _In_ Argument const& argument) noexcept
        {
            if (argument.Type == type::True)
            {
                return 1.0;
            }

            if (argument.Type == type::False)
            {
                return 0.0;
            }

            double value{ 0 };

            if (IsFloatType(argument.Type))
            {
                value = argument.Real;
            }
            else if (argument.Type == type::Int32 || argument.Type == type::Int64)
            {
                value = static_cast<double>(argument.Integer);
            }
            else
            {
                return std::nullopt;
            }

            if (!std::isfinite(value))
            {
                return std::nullopt;
            }

            return std::clamp((value - rule.Low) / (rule.High - rule.Low), 0.0, 1.0);
        }

        // The Custom format: the first rule whose address this is decides.
        Match TranslateCustom(_In_ OscRow const& row, _In_ std::string_view address, _In_ MessageView const& message, _Inout_ Output& output) noexcept
        {
            for (auto const& rule : row.Rules)
            {
                RuleParts parts{};

                if (rule.Direction == OscRuleDirection::Out || !MatchRuleAddress(rule.Address, address, parts))
                {
                    continue;
                }

                auto const channel = EvaluateRuleNumber(rule.Channel, parts);
                auto const number = EvaluateRuleNumber(rule.Number, parts);
                auto const usesNumber = rule.Message != OscRuleMessage::PitchBend && rule.Message != OscRuleMessage::ChannelPressure;

                if (!channel.has_value() || *channel < 0 || *channel > 15 ||
                    (usesNumber && (!number.has_value() || *number < 0 || *number > 127)) ||
                    rule.Argument >= message.ArgumentCount)
                {
                    return Match::Unreadable;
                }

                auto const unit = RuleUnit(rule, message.Arguments[rule.Argument]);

                if (!unit.has_value())
                {
                    return Match::Unreadable;
                }

                auto const value = static_cast<uint32_t>(std::llround(*unit * RuleHighest(rule.Message)));
                auto const ch = static_cast<uint8_t>(*channel);
                auto const n = static_cast<uint8_t>(usesNumber ? *number : 0);
                auto const v7 = static_cast<uint8_t>((std::min)(value, 127u));

                switch (rule.Message)
                {
                case OscRuleMessage::ControlChange:
                    output.Midi1(row.Group, static_cast<uint8_t>(0xB0 | ch), n, v7);
                    break;

                case OscRuleMessage::Note:
                    // The bottom of the range lets the note go.
                    output.Midi1(row.Group, static_cast<uint8_t>((v7 == 0 ? 0x80 : 0x90) | ch), n, v7);
                    break;

                case OscRuleMessage::ProgramChange:
                    // Like a button: the program changes when it's pressed, not when it's let go.
                    if (value > 0)
                    {
                        output.Midi1(row.Group, static_cast<uint8_t>(0xC0 | ch), n, 0);
                    }

                    break;

                case OscRuleMessage::PitchBend:
                    output.Midi1(row.Group, static_cast<uint8_t>(0xE0 | ch), static_cast<uint8_t>(value & 0x7F), static_cast<uint8_t>((value >> 7) & 0x7F));
                    break;

                case OscRuleMessage::ChannelPressure:
                    output.Midi1(row.Group, static_cast<uint8_t>(0xD0 | ch), v7, 0);
                    break;

                case OscRuleMessage::PolyPressure:
                    output.Midi1(row.Group, static_cast<uint8_t>(0xA0 | ch), n, v7);
                    break;
                }

                return Match::Done;
            }

            return Match::No;
        }

        Match TranslateLiteral(_In_ OscRow const& row, _In_ std::string_view address, _In_ MessageView const& message, _Inout_ Output& output) noexcept
        {
            switch (row.Format)
            {
            case OscFormat::Paths: return TranslatePaths(row, address, message, output);
            case OscFormat::MidiType: return TranslateMidiType(row, address, message, output);
            case OscFormat::Arguments: return TranslateArguments(row, address, message, output);
            case OscFormat::Custom: return TranslateCustom(row, address, message, output);
            }

            return Match::No;
        }

        // Builds addresses under a root, for matching a received pattern against.
        class AddressBuffer
        {
        public:
            explicit AddressBuffer(_In_ std::string_view root) noexcept
            {
                Append(root);
                m_rootLength = m_length;
            }

            void ToRoot() noexcept { m_length = m_rootLength; }

            AddressBuffer& Append(_In_ std::string_view text) noexcept
            {
                auto const room = m_text.size() - m_length;
                auto const count = (std::min)(text.size(), room);

                std::copy_n(text.data(), count, m_text.data() + m_length);
                m_length += count;
                m_overflow = m_overflow || count < text.size();

                return *this;
            }

            AddressBuffer& Append(_In_ uint32_t number) noexcept
            {
                std::array<char, 8> digits{};
                auto const result = std::to_chars(digits.data(), digits.data() + digits.size(), number);

                return Append(std::string_view{ digits.data(), static_cast<size_t>(result.ptr - digits.data()) });
            }

            std::string_view Text() const noexcept
            {
                return m_overflow ? std::string_view{} : std::string_view{ m_text.data(), m_length };
            }

        private:
            std::array<char, MaximumAddressLength + 1> m_text{};
            size_t m_length{ 0 };
            size_t m_rootLength{ 0 };
            bool m_overflow{ false };
        };

        // Every address a row takes, for a received pattern. Stops when visit says so.
        template <typename TVisit>
        void ForEachAddress(_In_ OscRow const& row, _In_ TVisit&& visit) noexcept
        {
            AddressBuffer buffer{ row.Root };

            auto const offer = [&]() noexcept { return visit(buffer.Text()); };

            switch (row.Format)
            {
            case OscFormat::Paths:
            {
                auto const first = row.ChannelsFromZero ? 0u : 1u;

                for (uint32_t channel = first; channel < first + 16; channel++)
                {
                    for (auto const& entry : PathChannelKinds)
                    {
                        buffer.ToRoot();
                        buffer.Append("/").Append(channel).Append("/").Append(entry.Name);

                        if (!offer()) return;
                    }

                    for (auto const& entry : PathNumberedKinds)
                    {
                        for (uint32_t number = 0; number < 128; number++)
                        {
                            buffer.ToRoot();
                            buffer.Append("/").Append(channel).Append("/").Append(entry.Name).Append("/").Append(number);

                            if (!offer()) return;
                        }
                    }
                }

                for (auto const& entry : SystemKinds)
                {
                    buffer.ToRoot();
                    buffer.Append("/").Append(entry.Name);

                    if (!offer()) return;
                }

                break;
            }

            case OscFormat::MidiType:
                buffer.ToRoot();
                if (!offer()) return;

                buffer.Append("/sysex");
                offer();
                break;

            case OscFormat::Arguments:
            {
                auto const visitNames = [&](auto const& names) noexcept
                    {
                        for (auto const& entry : names)
                        {
                            buffer.ToRoot();
                            buffer.Append("/").Append(entry.Name);

                            if (!offer()) return false;
                        }

                        return true;
                    };

                if (row.Names == OscArgumentNames::OpenStageControl ? visitNames(OpenStageControlKinds) : visitNames(OscMidiOscKinds))
                {
                    visitNames(SystemKinds);
                }

                break;
            }

            case OscFormat::Custom:
                // A rule with parts stands for more addresses than can be listed, so a pattern
                // only reaches the rules without any.
                for (auto const& rule : row.Rules)
                {
                    if (rule.Direction != OscRuleDirection::Out &&
                        rule.Address.find('{') == std::string::npos &&
                        !visit(std::string_view{ rule.Address }))
                    {
                        break;
                    }
                }

                break;
            }
        }

        float ToFloat(_In_ uint32_t value, _In_ uint32_t highest) noexcept
        {
            return static_cast<float>(value) / static_cast<float>(highest);
        }

        // A value as the row sends values.
        void AddValue(_Inout_ MessageBuilder& builder, _In_ OscRow const& row, _In_ uint32_t value, _In_ uint32_t highest)
        {
            switch (row.Values)
            {
            case OscValueKind::Integers: builder.Int(static_cast<int32_t>(value)); break;
            case OscValueKind::Floats: builder.Float(static_cast<float>(value)); break;
            case OscValueKind::UnitFloats: builder.Float(ToFloat(value, highest)); break;
            }
        }

        void AddPitchBend(_Inout_ MessageBuilder& builder, _In_ OscRow const& row, _In_ uint16_t value)
        {
            auto const signedValue = static_cast<int32_t>(value) - (row.PitchBend == OscPitchBendRange::Signed ? 8192 : 0);

            switch (row.Values)
            {
            case OscValueKind::Integers: builder.Int(signedValue); break;
            case OscValueKind::Floats: builder.Float(static_cast<float>(signedValue)); break;
            case OscValueKind::UnitFloats: builder.Float(UnitFromPitchBend(value)); break;
            }
        }

        std::string Address(_In_ OscRow const& row, _In_ std::string_view name)
        {
            std::string address{ row.Root };
            address += '/';
            address += name;
            return address;
        }

        std::string ChannelText(_In_ OscRow const& row, _In_ uint8_t channel)
        {
            return std::to_string(channel + (row.ChannelsFromZero ? 0 : 1));
        }

        std::string HexText(_In_ std::span<uint8_t const> bytes)
        {
            std::string text{};
            text.reserve(bytes.size() * 3);

            for (auto const b : bytes)
            {
                if (!text.empty())
                {
                    text += ' ';
                }

                text += "0123456789abcdef"[b >> 4];
                text += "0123456789abcdef"[b & 0x0F];
            }

            return text;
        }

        // The Custom format: the first rule that sends this kind of message, and whose channel and
        // number fit it, says the address. The value goes out as a float in the rule's range.
        void AppendCustom(_In_ OscRow const& row, _In_ Midi1Message const& midi, _Inout_ std::vector<MessageBuilder>& out)
        {
            auto const status = midi.Status;

            if (status < 0x80 || status >= 0xF0)
            {
                return;
            }

            auto const channel = static_cast<int32_t>(status & 0x0F);
            OscRuleMessage message{ OscRuleMessage::ControlChange };
            int32_t number{ midi.Data1 };
            uint32_t value{ midi.Data2 };

            switch (status & 0xF0)
            {
            case 0x80: message = OscRuleMessage::Note; value = 0; break;
            case 0x90: message = OscRuleMessage::Note; break;
            case 0xA0: message = OscRuleMessage::PolyPressure; break;
            case 0xB0: message = OscRuleMessage::ControlChange; break;
            case 0xC0: message = OscRuleMessage::ProgramChange; value = 127; break;
            case 0xD0: message = OscRuleMessage::ChannelPressure; value = midi.Data1; break;
            default: message = OscRuleMessage::PitchBend; value = static_cast<uint32_t>(midi.Data1) | (static_cast<uint32_t>(midi.Data2) << 7); break;
            }

            auto const usesNumber = message != OscRuleMessage::PitchBend && message != OscRuleMessage::ChannelPressure;

            for (auto const& rule : row.Rules)
            {
                if (rule.Direction == OscRuleDirection::In || rule.Message != message)
                {
                    continue;
                }

                RuleParts parts{};

                if (!SolveRuleNumber(rule.Channel, channel, parts) || (usesNumber && !SolveRuleNumber(rule.Number, number, parts)))
                {
                    continue;
                }

                auto const address = BuildRuleAddress(rule.Address, parts);

                if (!address.has_value())
                {
                    continue;
                }

                auto const unit = static_cast<double>(value) / RuleHighest(rule.Message);
                out.emplace_back(*address).Float(static_cast<float>(rule.Low + unit * (rule.High - rule.Low)));
                return;
            }
        }

        // One MIDI 1.0 message as the row's format says it.
        void AppendMidi1(_In_ OscRow const& row, _In_ Midi1Message const& midi, _Inout_ std::vector<MessageBuilder>& out)
        {
            auto const status = midi.Status;

            if (row.Format == OscFormat::Custom)
            {
                AppendCustom(row, midi, out);
                return;
            }

            if (row.Format == OscFormat::MidiType)
            {
                auto const root = row.Root.empty() ? std::string{ "/" } : row.Root;
                out.emplace_back(root).Midi(row.Group, status, midi.Data1, midi.Data2);
                return;
            }

            if (status >= 0xF0)
            {
                Kind kind{ Kind::Clock };

                switch (status)
                {
                case 0xF1: kind = Kind::TimeCode; break;
                case 0xF2: kind = Kind::SongPosition; break;
                case 0xF3: kind = Kind::SongSelect; break;
                case 0xF6: kind = Kind::TuneRequest; break;
                case 0xF8: kind = Kind::Clock; break;
                case 0xFA: kind = Kind::Start; break;
                case 0xFB: kind = Kind::Continue; break;
                case 0xFC: kind = Kind::Stop; break;
                case 0xFE: kind = Kind::ActiveSensing; break;
                case 0xFF: kind = Kind::Reset; break;
                default: return;
                }

                auto& builder = out.emplace_back(Address(row, NameOf(SystemKinds, kind)));

                if (kind == Kind::SongPosition)
                {
                    builder.Int(midi.Data1 | (midi.Data2 << 7));
                }
                else if (kind == Kind::SongSelect || kind == Kind::TimeCode)
                {
                    builder.Int(midi.Data1);
                }

                return;
            }

            auto const channel = static_cast<uint8_t>(status & 0x0F);
            auto kind = Kind::Note;
            auto velocity = midi.Data2;

            switch (status & 0xF0)
            {
            case 0x80:
                kind = row.NoteOff == OscNoteOff::OwnAddress ? Kind::NoteOff : Kind::Note;
                velocity = row.NoteOff == OscNoteOff::OwnAddress ? midi.Data2 : 0;
                break;

            case 0x90:
                kind = midi.Data2 == 0 && row.NoteOff == OscNoteOff::OwnAddress ? Kind::NoteOff : Kind::Note;
                break;

            case 0xA0: kind = Kind::PolyPressure; break;
            case 0xB0: kind = Kind::ControlChange; break;
            case 0xC0: kind = Kind::Program; break;
            case 0xD0: kind = Kind::ChannelPressure; break;
            case 0xE0: kind = Kind::PitchBend; break;
            default: return;
            }

            auto const hasNumber = kind == Kind::Note || kind == Kind::NoteOff || kind == Kind::PolyPressure || kind == Kind::ControlChange;
            auto const bend = static_cast<uint16_t>(midi.Data1 | (midi.Data2 << 7));

            auto const addValue = [&](MessageBuilder& builder)
                {
                    if (kind == Kind::PitchBend)
                    {
                        AddPitchBend(builder, row, bend);
                    }
                    else if (kind == Kind::Program || kind == Kind::ChannelPressure)
                    {
                        AddValue(builder, row, midi.Data1, 127);
                    }
                    else
                    {
                        AddValue(builder, row, kind == Kind::Note || kind == Kind::NoteOff ? velocity : midi.Data2, 127);
                    }
                };

            if (row.Format == OscFormat::Paths)
            {
                std::string address{ row.Root };
                address += '/';
                address += ChannelText(row, channel);
                address += '/';
                address += hasNumber ? NameOf(PathNumberedKinds, kind) : NameOf(PathChannelKinds, kind);

                if (hasNumber)
                {
                    address += '/';
                    address += std::to_string(midi.Data1);
                }

                addValue(out.emplace_back(address));
                return;
            }

            if (row.Names == OscArgumentNames::OpenStageControl)
            {
                auto& builder = out.emplace_back(Address(row, NameOf(OpenStageControlKinds, kind)));
                builder.Int(channel + (row.ChannelsFromZero ? 0 : 1));

                if (hasNumber)
                {
                    builder.Int(midi.Data1);
                }

                addValue(builder);
                return;
            }

            // OSC_MIDI_OSC has no channel, and its note off carries only the note.
            auto& builder = out.emplace_back(Address(row, NameOf(OscMidiOscKinds, kind)));

            if (hasNumber)
            {
                builder.Int(midi.Data1);
            }

            if (kind != Kind::NoteOff)
            {
                addValue(builder);
            }
        }

        void AppendSysEx(_In_ OscRow const& row, _In_ std::span<uint8_t const> bytes, _Inout_ std::vector<MessageBuilder>& out)
        {
            auto& builder = out.emplace_back(Address(row, "sysex"));

            if (row.Format == OscFormat::Arguments && row.Names == OscArgumentNames::OpenStageControl)
            {
                builder.String(HexText(bytes));
            }
            else
            {
                builder.Blob(bytes);
            }
        }
    }

    _Use_decl_annotations_
    float UnitFromPitchBend(uint16_t value) noexcept
    {
        value = (std::min)(value, uint16_t{ 16383 });

        if (value <= 8192)
        {
            return static_cast<float>(value) / 8192.0f * 0.5f;
        }

        return 0.5f + static_cast<float>(value - 8192) / 8191.0f * 0.5f;
    }

    _Use_decl_annotations_
    uint16_t PitchBendFromUnit(double unit) noexcept
    {
        if (!std::isfinite(unit))
        {
            return 8192;
        }

        unit = std::clamp(unit, 0.0, 1.0);

        if (unit <= 0.5)
        {
            return static_cast<uint16_t>(std::llround(unit * 2.0 * 8192.0));
        }

        return static_cast<uint16_t>(8192 + std::llround((unit - 0.5) * 2.0 * 8191.0));
    }

    _Use_decl_annotations_
    InboundOutcome TranslateInbound(std::span<OscRow const> rows, MessageView const& message, UmpReceiver& receiver) noexcept
    {
        Output output{ receiver };
        auto matched = false;
        auto unreadable = false;

        auto const consider = [&](OscRow const& row, std::string_view address) noexcept
            {
                switch (TranslateLiteral(row, address, message, output))
                {
                case Match::Done: matched = true; break;
                case Match::Unreadable: matched = true; unreadable = true; break;
                case Match::No: break;
                }
            };

        if (HasPatternCharacters(message.Address))
        {
            for (auto const& row : rows)
            {
                ForEachAddress(row, [&](std::string_view candidate) noexcept
                    {
                        if (!candidate.empty() && PatternMatches(message.Address, candidate))
                        {
                            consider(row, candidate);
                        }

                        return !output.Capped;
                    });

                if (output.Capped)
                {
                    break;
                }
            }
        }
        else
        {
            for (auto const& row : rows)
            {
                consider(row, message.Address);
            }
        }

        InboundOutcome outcome{};
        outcome.Messages = output.Messages;

        if (output.Capped)
        {
            outcome.Result = InboundResult::TooManyMessages;
        }
        else if (!matched)
        {
            outcome.Result = InboundResult::NoRowMatched;
        }
        else if (unreadable && output.Messages == 0)
        {
            outcome.Result = InboundResult::Unreadable;
        }
        else
        {
            outcome.Result = InboundResult::Translated;
        }

        return outcome;
    }

    _Use_decl_annotations_
    size_t Midi1FromMidi2(uint32_t const* words, Midi1Message* out) noexcept
    {
        auto const word0 = words[0];
        auto const word1 = words[1];
        auto const opcode = static_cast<uint8_t>((word0 >> 20) & 0x0F);
        auto const channel = static_cast<uint8_t>((word0 >> 16) & 0x0F);
        auto const index = static_cast<uint8_t>((word0 >> 8) & 0x7F);

        auto const cc = [&](uint8_t controller, uint8_t value) noexcept
            {
                return Midi1Message{ static_cast<uint8_t>(0xB0 | channel), controller, static_cast<uint8_t>(value & 0x7F) };
            };

        switch (opcode)
        {
        case 0x8:
        case 0x9:
        {
            auto velocity = static_cast<uint8_t>((word1 >> 16) >> 9);

            // A MIDI 1.0 note on with velocity 0 is a note off, so the least a note on gets is 1.
            if (opcode == 0x9 && velocity == 0)
            {
                velocity = 1;
            }

            out[0] = Midi1Message{ static_cast<uint8_t>((opcode << 4) | channel), index, velocity };
            return 1;
        }

        case 0xA:
        case 0xB:
            out[0] = Midi1Message{ static_cast<uint8_t>((opcode << 4) | channel), index, static_cast<uint8_t>(word1 >> 25) };
            return 1;

        case 0xC:
        {
            auto const program = static_cast<uint8_t>((word1 >> 24) & 0x7F);
            size_t count{ 0 };

            if ((word0 & 0x1) != 0)
            {
                out[count++] = cc(0, static_cast<uint8_t>((word1 >> 8) & 0x7F));
                out[count++] = cc(32, static_cast<uint8_t>(word1 & 0x7F));
            }

            out[count++] = Midi1Message{ static_cast<uint8_t>(0xC0 | channel), program, 0 };
            return count;
        }

        case 0xD:
            out[0] = Midi1Message{ static_cast<uint8_t>(0xD0 | channel), static_cast<uint8_t>(word1 >> 25), 0 };
            return 1;

        case 0xE:
        {
            auto const bend = word1 >> 18;
            out[0] = Midi1Message{ static_cast<uint8_t>(0xE0 | channel), static_cast<uint8_t>(bend & 0x7F), static_cast<uint8_t>((bend >> 7) & 0x7F) };
            return 1;
        }

        case 0x2:
        case 0x3:
        {
            auto const bank = static_cast<uint8_t>((word0 >> 8) & 0x7F);
            auto const number = static_cast<uint8_t>(word0 & 0x7F);
            auto const registered = opcode == 0x2;

            out[0] = cc(registered ? 101 : 99, bank);
            out[1] = cc(registered ? 100 : 98, number);
            out[2] = cc(6, static_cast<uint8_t>(word1 >> 25));
            out[3] = cc(38, static_cast<uint8_t>((word1 >> 18) & 0x7F));
            return 4;
        }

        default:
            return 0;
        }
    }

    _Use_decl_annotations_
    OutboundResult OutboundTranslator::Translate(
        std::span<OscRow const> rows,
        uint32_t const* words,
        uint8_t wordCount,
        std::vector<MessageBuilder>& out)
    {
        if (words == nullptr || wordCount == 0)
        {
            return OutboundResult::NotSendable;
        }

        auto const type = words[0] >> 28;

        if (type != 0x1 && type != 0x2 && type != 0x3 && type != 0x4)
        {
            return OutboundResult::NotSendable;
        }

        auto const group = static_cast<uint8_t>((words[0] >> 24) & 0x0F);

        OscRow const* row{ nullptr };

        for (auto const& candidate : rows)
        {
            if (candidate.Group == group)
            {
                row = &candidate;
                break;
            }
        }

        if (row == nullptr)
        {
            return OutboundResult::NoRow;
        }

        auto const before = out.size();

        if (type == 0x1)
        {
            auto const status = static_cast<uint8_t>(words[0] >> 16);

            if ((status == 0xF8 || status == 0xFE) && !row->SendClock)
            {
                return OutboundResult::LeftOut;
            }

            AppendMidi1(*row, Midi1Message{ status, static_cast<uint8_t>((words[0] >> 8) & 0x7F), static_cast<uint8_t>(words[0] & 0x7F) }, out);
            return out.size() > before ? OutboundResult::Sent : OutboundResult::NotSendable;
        }

        if (type == 0x2)
        {
            AppendMidi1(*row, Midi1Message{ static_cast<uint8_t>(words[0] >> 16), static_cast<uint8_t>((words[0] >> 8) & 0x7F), static_cast<uint8_t>(words[0] & 0x7F) }, out);
            return out.size() > before ? OutboundResult::Sent : OutboundResult::NotSendable;
        }

        if (wordCount < 2)
        {
            return OutboundResult::NotSendable;
        }

        if (type == 0x4)
        {
            std::array<Midi1Message, 4> messages{};
            auto const count = Midi1FromMidi2(words, messages.data());

            if (count == 0)
            {
                return OutboundResult::NotSendable;
            }

            for (size_t i = 0; i < count; i++)
            {
                AppendMidi1(*row, messages[i], out);
            }

            return OutboundResult::Sent;
        }

        // SysEx7. Collected whole, whether or not the row sends it, so a part in the middle is
        // never mistaken for a start.
        auto& progress = m_sysEx[group];
        auto const status = (words[0] >> 20) & 0x0F;
        auto const count = (std::min)((words[0] >> 16) & 0x0F, 6u);

        uint8_t const data[6]
        {
            static_cast<uint8_t>((words[0] >> 8) & 0x7F),
            static_cast<uint8_t>(words[0] & 0x7F),
            static_cast<uint8_t>((words[1] >> 24) & 0x7F),
            static_cast<uint8_t>((words[1] >> 16) & 0x7F),
            static_cast<uint8_t>((words[1] >> 8) & 0x7F),
            static_cast<uint8_t>(words[1] & 0x7F),
        };

        if (status == 0x0 || status == 0x1)
        {
            // A start before the last one ended loses the last one. This one still goes.
            auto const interrupted = progress.Collecting;

            progress.Bytes.clear();
            progress.Bytes.push_back(0xF0);
            progress.Collecting = true;
            progress.TooLarge = false;
            progress.Bytes.insert(progress.Bytes.end(), data, data + count);

            if (status == 0x1)
            {
                return interrupted ? OutboundResult::SysExNotSent : OutboundResult::Waiting;
            }
        }
        else if (!progress.Collecting)
        {
            return OutboundResult::SysExNotSent;
        }
        else if (!progress.TooLarge)
        {
            if (progress.Bytes.size() + count + 1 > MaximumSysExBytes)
            {
                progress.TooLarge = true;
                progress.Bytes.clear();
            }
            else
            {
                progress.Bytes.insert(progress.Bytes.end(), data, data + count);
            }
        }

        if (status == 0x2)
        {
            return OutboundResult::Waiting;
        }

        progress.Collecting = false;

        if (progress.TooLarge)
        {
            return OutboundResult::SysExNotSent;
        }

        if (!row->SendSysEx || row->Format == OscFormat::Custom)
        {
            return OutboundResult::LeftOut;
        }

        progress.Bytes.push_back(0xF7);
        AppendSysEx(*row, progress.Bytes, out);

        return OutboundResult::Sent;
    }

    void OutboundTranslator::Reset() noexcept
    {
        for (auto& progress : m_sysEx)
        {
            progress.Collecting = false;
            progress.TooLarge = false;
            progress.Bytes.clear();
        }
    }

    _Use_decl_annotations_
    std::wstring ArgumentsText(MessageView const& message)
    {
        std::wstring text{};

        for (size_t i = 0; i < message.ArgumentCount; i++)
        {
            auto const& argument = message.Arguments[i];

            if (!text.empty())
            {
                text += L", ";
            }

            text += static_cast<wchar_t>(argument.Type);

            switch (argument.Type)
            {
            case type::Int32:
            case type::Int64:
                text += std::format(L" {}", argument.Integer);
                break;

            case type::Float32:
            case type::Double:
                text += std::format(L" {:g}", argument.Real);
                break;

            case type::String:
            case type::Symbol:
            {
                std::wstring value{};

                // Only printable ASCII is shown, and only the start of a long string.
                for (auto const c : argument.Text.substr(0, 48))
                {
                    value += c >= 0x20 && c < 0x7F ? static_cast<wchar_t>(c) : L'?';
                }

                text += L" " + value;
                break;
            }

            case type::Blob:
                text += std::format(L" ({} bytes)", argument.Bytes.size());
                break;

            case type::Midi:
            {
                auto const packed = static_cast<uint32_t>(argument.Integer);
                text += std::format(L" {:02X} {:02X} {:02X} {:02X}", packed >> 24, (packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF);
                break;
            }

            case type::Character:
                text += std::format(L" {}", argument.Integer & 0x7F);
                break;

            default:
                break;
            }
        }

        return text;
    }

    _Use_decl_annotations_
    std::optional<OscRow> SuggestRow(MessageView const& message, uint8_t group)
    {
        std::string_view const address = message.Address;

        if (address.empty() || HasPatternCharacters(address))
        {
            return std::nullopt;
        }

        // One m argument: the MIDI type, at this address.
        for (size_t i = 0; i < message.ArgumentCount; i++)
        {
            if (message.Arguments[i].Type == type::Midi)
            {
                if (!IsValidRoot(address))
                {
                    return std::nullopt;
                }

                auto row = OscRowFromPreset(OscPreset::MidiType, group);
                row.Root = std::string{ address };
                row.Group = static_cast<uint8_t>((static_cast<uint32_t>(message.Arguments[i].Integer) >> 24) & 0x0F);
                return row;
            }
        }

        // Find where a known name starts, and take what's before it as the root.
        std::array<std::string_view, MaximumAddressParts> parts{};
        size_t count{ 0 };
        std::string_view rest = address.substr(1);

        while (count < parts.size())
        {
            auto const slash = rest.find('/');
            parts[count++] = rest.substr(0, slash);

            if (slash == std::string_view::npos)
            {
                break;
            }

            rest.remove_prefix(slash + 1);
        }

        auto const rootBefore = [&](size_t part) noexcept
            {
                size_t length{ 0 };

                for (size_t i = 0; i < part; i++)
                {
                    length += 1 + parts[i].size();
                }

                return address.substr(0, length);
            };

        for (size_t i = 0; i < count; i++)
        {
            auto const name = parts[i];

            // <root>/<channel>/note/<number> and the rest of the paths.
            if (i >= 1 && (FindKind(PathNumberedKinds, name).has_value() || FindKind(PathChannelKinds, name).has_value()))
            {
                auto const channel = ParseDecimal(parts[i - 1]);

                if (channel.has_value() && *channel <= 16)
                {
                    auto row = OscRowFromPreset(OscPreset::Paths, group);
                    row.Root = std::string{ rootBefore(i - 1) };
                    row.ChannelsFromZero = *channel == 0;

                    if (row.Root == "/vkb_midi")
                    {
                        row = OscRowFromPreset(OscPreset::ReaperVirtualKeyboard, group);
                    }

                    if (IsValidRoot(row.Root))
                    {
                        return row;
                    }
                }
            }

            if (i + 1 == count && message.ArgumentCount >= 1)
            {
                if (FindKind(OscMidiOscKinds, name).has_value())
                {
                    auto row = OscRowFromPreset(OscPreset::OscMidiOsc, group);
                    row.Root = std::string{ rootBefore(i) };
                    return IsValidRoot(row.Root) ? std::optional<OscRow>{ row } : std::nullopt;
                }

                if (FindKind(OpenStageControlKinds, name).has_value() && message.ArgumentCount >= 2)
                {
                    auto row = OscRowFromPreset(OscPreset::OpenStageControl, group);
                    row.Root = std::string{ rootBefore(i) };
                    return IsValidRoot(row.Root) ? std::optional<OscRow>{ row } : std::nullopt;
                }
            }
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    std::optional<OscRule> SuggestRule(MessageView const& message, std::span<OscRow const> rows)
    {
        auto const address = message.Address;

        if (HasPatternCharacters(address))
        {
            return std::nullopt;
        }

        // The value is the first number, true or false.
        size_t argument{ 0 };

        while (argument < message.ArgumentCount && !IsNumberType(message.Arguments[argument].Type))
        {
            argument++;
        }

        if (argument >= message.ArgumentCount || argument > MaximumOscRuleArgument)
        {
            return std::nullopt;
        }

        OscRule rule{};
        rule.Argument = static_cast<uint8_t>(argument);

        // A number at the end, as in /1/fader3, becomes a part, so one rule takes the whole set.
        auto digits = address.size();

        while (digits > 0 && address[digits - 1] >= '0' && address[digits - 1] <= '9')
        {
            digits--;
        }

        auto const numbered = digits < address.size() && address.size() - digits <= 6;

        rule.Address = numbered ? std::string{ address.substr(0, digits) } + "{n}" : std::string{ address };

        if (!IsValidRuleAddress(rule.Address))
        {
            return std::nullopt;
        }

        // TouchOSC's controls go from 0 to 1. Whole numbers past 1 are taken as MIDI values.
        auto const& value = message.Arguments[argument];
        auto const beyondOne =
            (IsFloatType(value.Type) && std::isfinite(value.Real) && (value.Real > 1.0 || value.Real < 0.0)) ||
            ((value.Type == type::Int32 || value.Type == type::Int64) && (value.Integer > 1 || value.Integer < 0));

        rule.Low = 0.0;
        rule.High = beyondOne ? 127.0 : 1.0;

        // What the control seems to be, by its name: buttons play notes, switches are on/off
        // controllers, and anything else, like a fader, is a controller.
        std::string name{};
        auto const slash = address.rfind('/', digits == 0 ? 0 : digits - 1);

        for (auto const c : address.substr(slash == std::string_view::npos ? 0 : slash + 1, digits - (slash == std::string_view::npos ? 0 : slash + 1)))
        {
            name += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        auto const has = [&name](std::initializer_list<std::string_view> words)
            {
                return std::any_of(words.begin(), words.end(), [&name](std::string_view word) { return name.find(word) != std::string::npos; });
            };

        int32_t first{ 71 };

        if (has({ "push", "button", "pad", "key", "note", "drum" }))
        {
            rule.Message = OscRuleMessage::Note;
            first = 60;
        }
        else if (has({ "toggle", "switch" }))
        {
            first = 80;
        }

        // Keep clear of the numbers other rules use for the same kind of message: a part's
        // numbers for parts 1 to 8, or a fixed number. TouchOSC's pages have up to 8 of a kind.
        constexpr int32_t ReservedParts = 8;
        std::array<bool, 128> used{};

        for (auto const& row : rows)
        {
            for (auto const& other : row.Rules)
            {
                auto const sameKind = (other.Message == OscRuleMessage::Note) == (rule.Message == OscRuleMessage::Note);

                if (!sameKind || other.Message == OscRuleMessage::PitchBend || other.Message == OscRuleMessage::ChannelPressure)
                {
                    continue;
                }

                auto const fixed = other.Number.Part.empty();

                for (int32_t part = fixed ? 0 : 1; part <= (fixed ? 0 : ReservedParts); part++)
                {
                    auto const number = other.Number.Offset + part;

                    if (number >= 0 && number < 128)
                    {
                        used[static_cast<size_t>(number)] = true;
                    }
                }
            }
        }

        // A part counts from 1, the way controls are numbered, so {n}+70 makes fader 1 controller 71.
        auto const span = numbered ? ReservedParts : 1;
        auto const lowest = numbered ? 1 : 0;

        auto const isFree = [&used, span](int32_t start)
            {
                if (start < 0 || start + span > 128)
                {
                    return false;
                }

                for (int32_t i = start; i < start + span; i++)
                {
                    if (used[static_cast<size_t>(i)])
                    {
                        return false;
                    }
                }

                return true;
            };

        auto start = first;

        for (int32_t tries = 0; tries < 128 && !isFree(start); tries++)
        {
            start = (start + span) % 128;
        }

        rule.Number = numbered ? OscRuleNumber{ "n", start - lowest } : OscRuleNumber{ {}, start };

        return rule;
    }
}
