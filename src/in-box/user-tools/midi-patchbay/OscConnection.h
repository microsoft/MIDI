// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// An OSC connection: a node on the canvas that sends and receives MIDI as Open Sound Control
// messages over UDP. Its rows are address spaces, the OSC version of a device's groups: each row
// has an address root, a format for what goes under it, and the group its messages come in on and
// go out from. Pure: no XAML, no resources and no precompiled header, so the unit tests compile it
// exactly as it ships.

#include <sal.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

namespace midipatchbay
{
    namespace json = ::winrt::Windows::Data::Json;

    // What goes under a row's root.
    enum class OscFormat : int32_t
    {
        // The address says where and the argument says how much: <root>/<channel>/note/<note>
        // with the velocity as the argument. The names are REAPER's.
        Paths = 0,

        // One argument of OSC's MIDI type, m: port id, status, data 1, data 2. The port id is the
        // row's group.
        MidiType = 1,

        // Fixed names under the root, and every number an argument: <root>/note with the
        // channel, note and velocity as arguments.
        Arguments = 2,

        // Rules, each one OSC address to one kind of MIDI message, for apps that send addresses
        // of their own, like a TouchOSC fader at /1/fader1. The root isn't used.
        Custom = 3,
    };

    constexpr int32_t OscFormatCount = 4;

    // Which way a rule works.
    enum class OscRuleDirection : int32_t
    {
        Both = 0,

        // OSC that arrives becomes MIDI.
        In = 1,

        // MIDI going out becomes OSC.
        Out = 2,
    };

    enum class OscRuleMessage : int32_t
    {
        ControlChange = 0,

        // The value is the velocity. The lowest value is a note off.
        Note = 1,

        // Sent when the value isn't the lowest, like a button that's pressed.
        ProgramChange = 2,

        PitchBend = 3,
        ChannelPressure = 4,
        PolyPressure = 5,
    };

    constexpr int32_t OscRuleMessageCount = 6;

    // A channel or a number in a rule: a part of the address, such as the 3 in /1/fader3 when the
    // rule says /1/fader{n}, plus or minus an offset. With no part, it's the offset alone.
    struct OscRuleNumber
    {
        // Lowercase letters, as in {n}. Empty for a fixed number.
        std::string Part{};

        int32_t Offset{ 0 };
    };

    struct OscRule
    {
        // An OSC address with parts in braces where a number goes: "/1/fader{n}". ASCII only.
        std::string Address{};

        // Which argument carries the value, counted from 0.
        uint8_t Argument{ 0 };

        OscRuleDirection Direction{ OscRuleDirection::Both };
        OscRuleMessage Message{ OscRuleMessage::ControlChange };

        // 0 to 15 once worked out.
        OscRuleNumber Channel{};

        // The controller, note or program, 0 to 127 once worked out. Pitch bend and channel
        // pressure don't use it.
        OscRuleNumber Number{ {}, 1 };

        // The OSC values that stand for the lowest and the highest MIDI value. High below Low
        // turns it upside down.
        double Low{ 0.0 };
        double High{ 1.0 };
    };

    constexpr size_t MaximumOscRules = 64;
    constexpr size_t MaximumOscRuleParts = 4;
    constexpr uint8_t MaximumOscRuleArgument = 7;
    constexpr double MaximumOscRuleValue = 1.0e9;

    // The names the Arguments format uses.
    enum class OscArgumentNames : int32_t
    {
        // /note, /control, /program, /pitch... with the channel first.
        OpenStageControl = 0,

        // /noteOn, /cc, /ProgramChange... with no channel at all.
        OscMidiOsc = 1,
    };

    // How numbers go out. Whatever this says, received numbers are read generously.
    enum class OscValueKind : int32_t
    {
        Integers = 0,

        // The same numbers as floats.
        Floats = 1,

        // Floats from 0 to 1, the way TouchOSC's controls work. Pitch bend's center is 0.5.
        UnitFloats = 2,
    };

    enum class OscPitchBendRange : int32_t
    {
        // 0 to 16383, center 8192: the MIDI value as it is.
        Unsigned = 0,

        // -8192 to 8191, center 0.
        Signed = 1,
    };

    enum class OscNoteOff : int32_t
    {
        // A note with velocity 0, which is REAPER's rule.
        VelocityZero = 0,

        // An address of its own, with the release velocity.
        OwnAddress = 1,
    };

    enum class OscTransport : int32_t
    {
        // Each OSC packet on its own. What most apps use.
        Udp = 0,

        // TCP, connecting to the send host and port. Messages come back on the same connection.
        TcpConnect = 1,

        // TCP, waiting on the receive port for apps to connect. Sends to every app connected.
        TcpListen = 2,
    };

    // How packets are marked in a TCP stream.
    enum class OscTcpFraming : int32_t
    {
        // OSC 1.1: SLIP, as TouchOSC and most newer apps do.
        Slip = 0,

        // OSC 1.0: each packet's size first.
        SizeFirst = 1,
    };

    enum class OscListenOn : int32_t
    {
        EveryNetwork = 0,
        ThisPc = 1,
    };

    enum class OscAcceptFrom : int32_t
    {
        Anyone = 0,

        // The computer the connection sends to. Anyone when it doesn't send.
        SendHost = 1,

        // The addresses in the list.
        List = 2,
    };

    enum class OscReceivedTimeTags : int32_t
    {
        // Every message plays when it arrives. Works whatever the two clocks say.
        PlayOnArrival = 0,

        // A bundle plays at the time its tag says. Right only when both clocks are synced.
        UseSenderTime = 1,

        // Bundles keep the spacing the sender gave them, a little later than they arrive.
        KeepSenderSpacing = 2,
    };

    enum class OscSendTiming : int32_t
    {
        // A message meant for later is held until then.
        AtItsTime = 0,

        // A message meant for later goes now, in a bundle tagged with its time.
        EarlyWithTimeTags = 1,
    };

    // Presets fill in every setting on a row in one go.
    enum class OscPreset : int32_t
    {
        ReaperVirtualKeyboard = 0,
        Paths = 1,
        MidiType = 2,
        OpenStageControl = 3,
        OscMidiOsc = 4,

        // Custom rules for TouchOSC's own layouts: faders to controllers, toggles to the
        // on/off controllers, and push buttons to notes.
        TouchOsc = 5,
    };

    constexpr int32_t OscPresetCount = 6;

    struct OscRow
    {
        // 0 to 15. Received messages come in on it, and the m format's port id is it. Each row of
        // a connection has a group of its own.
        uint8_t Group{ 0 };

        // "/vkb_midi", or empty for the top level. ASCII only, as OSC addresses are.
        std::string Root{ "/midi" };

        OscFormat Format{ OscFormat::Paths };
        OscArgumentNames Names{ OscArgumentNames::OpenStageControl };

        // Channels counted 0 to 15 in addresses and arguments. Otherwise 1 to 16.
        bool ChannelsFromZero{ false };

        OscValueKind Values{ OscValueKind::Integers };
        OscPitchBendRange PitchBend{ OscPitchBendRange::Unsigned };
        OscNoteOff NoteOff{ OscNoteOff::VelocityZero };

        bool SendSysEx{ true };

        // Timing clock and active sensing. Clock is 48 packets a second at 120 BPM.
        bool SendClock{ false };

        // 0 to 15: the channel received messages come in on when they carry none. REAPER takes
        // addresses without a channel, and the OSC_MIDI_OSC names never have one.
        uint8_t Channel{ 0 };

        // The Custom format's rules, in the order they're tried.
        std::vector<OscRule> Rules{};
    };

    constexpr size_t MaximumOscRows = 16;
    constexpr size_t MaximumOscAcceptAddresses = 16;
    constexpr size_t MaximumOscHostLength = 253;

    constexpr uint32_t DefaultOscReceiveRateLimit = 5000;
    constexpr uint32_t MinimumOscReceiveRateLimit = 10;
    constexpr uint32_t MaximumOscReceiveRateLimit = 100000;

    constexpr uint32_t DefaultOscSpacingDelayMilliseconds = 10;
    constexpr uint32_t MaximumOscSpacingDelayMilliseconds = 500;

    // Apps connected at once to a connection that waits for them over TCP. More are turned away.
    constexpr size_t MaximumOscTcpApps = 8;

    struct OscConnectionSettings
    {
        OscTransport Transport{ OscTransport::Udp };
        OscTcpFraming TcpFraming{ OscTcpFraming::Slip };

        // 0 doesn't receive. Not used when connecting over TCP, which receives on the connection.
        uint16_t ReceivePort{ 0 };

        OscListenOn ListenOn{ OscListenOn::EveryNetwork };
        OscAcceptFrom AcceptFrom{ OscAcceptFrom::SendHost };
        std::vector<std::wstring> AcceptAddresses{};

        // A name, an IP address or a .local name. Empty doesn't send.
        std::wstring SendHost{};

        // 0 doesn't send.
        uint16_t SendPort{ 0 };

        // Sends to the computer that sent to this connection last, at SendPort. For phones and
        // tablets whose address changes.
        bool SendToLastSender{ false };

        OscReceivedTimeTags ReceivedTimeTags{ OscReceivedTimeTags::PlayOnArrival };
        uint32_t SpacingDelayMilliseconds{ DefaultOscSpacingDelayMilliseconds };

        OscSendTiming SendTiming{ OscSendTiming::AtItsTime };

        // Messages that play at the same moment go out in one bundle.
        bool BundleTogether{ false };

        // Received messages a second, beyond which the rest are dropped and counted.
        uint32_t ReceiveRateLimit{ DefaultOscReceiveRateLimit };

        // Tells apps on the network that it's here, by DNS-SD, so an app like TouchOSC can find
        // it. Only while it listens on every network and Windows Firewall lets it in.
        bool Advertise{ true };

        std::vector<OscRow> Rows{};
    };

    // A row as the preset makes it, on the given group.
    OscRow OscRowFromPreset(_In_ OscPreset preset, _In_ uint8_t group) noexcept;

    // The preset a row matches exactly, when there is one.
    std::optional<OscPreset> OscPresetOf(_In_ OscRow const& row) noexcept;

    // A new connection with one row, made from the preset.
    OscConnectionSettings OscConnectionFromPreset(_In_ OscPreset preset) noexcept;

    // Whether the connection receives, sends, or both, as its settings stand.
    bool OscConnectionReceives(_In_ OscConnectionSettings const& settings) noexcept;
    bool OscConnectionSends(_In_ OscConnectionSettings const& settings) noexcept;

    // Whether it waits on its receive port for others, over UDP or TCP: what Windows Firewall
    // and the "listen on" setting are about.
    bool OscConnectionListens(_In_ OscConnectionSettings const& settings) noexcept;

    // The row on this group, or null.
    OscRow const* OscRowOnGroup(_In_ OscConnectionSettings const& settings, _In_ uint8_t group) noexcept;

    // The lowest group no row uses, or nothing when all sixteen are used.
    std::optional<uint8_t> FreeOscGroup(_In_ OscConnectionSettings const& settings) noexcept;

    // The address a row puts its messages under, for people to read: "/vkb_midi", or "/" for
    // the top level.
    std::wstring OscRowAddressText(_In_ OscRow const& row);

    // Brings everything into range: rows past the limit, rows sharing a group, roots that aren't
    // valid OSC addresses, and out-of-range numbers. Used on everything read from a file.
    void SanitizeOscConnection(_Inout_ OscConnectionSettings& settings) noexcept;

    // The patch file's keys. Values are written only when they differ from the defaults, except
    // the ones every connection has.
    json::JsonObject OscConnectionToJson(_In_ OscConnectionSettings const& settings);
    OscConnectionSettings OscConnectionFromJson(_In_ json::JsonObject const& object) noexcept;

    // Equal for two connections that behave the same way.
    std::wstring OscConnectionSignature(_In_ OscConnectionSettings const& settings) noexcept;

    // The DNS-SD service types OSC is advertised and found under.
    constexpr wchar_t OscUdpServiceType[] = L"_osc._udp.local";
    constexpr wchar_t OscTcpServiceType[] = L"_osc._tcp.local";

    // The name a connection is advertised under: "TouchOSC (STUDIO-PC)". A DNS label: no
    // periods, no control characters, and at most 63 bytes of UTF-8, cut where a character
    // ends. Empty names fall back to the computer's name alone.
    std::wstring OscServiceLabel(_In_ std::wstring_view connectionName, _In_ std::wstring_view computerName);

    // The names a patch file uses for each enum.
    std::wstring_view OscFormatKey(_In_ OscFormat format) noexcept;
    std::wstring_view OscPresetKey(_In_ OscPreset preset) noexcept;
}
