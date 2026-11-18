// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Novation Launchpad X controller orchestration, endpoint/group selection, and pure output sink
// interface (pure C++20, PCH-free, XAML-free, WinRT-free), plus optional WinRT MidiSession
// transport adapter when WINRT_Windows_Devices_Midi2_H is defined.

#include <sal.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "LaunchpadXProtocol.h"
#include "LaunchpadXSurface.h"

namespace midisequencer::launchpad
{
    inline constexpr uint16_t NovationUsbVendorId = 0x1235u;

    class ILaunchpadOutput
    {
    public:
        virtual ~ILaunchpadOutput() = default;
        virtual bool SendUmpPackets(_In_ std::span<const UmpPacket> packets) noexcept = 0;
    };

    struct LaunchpadEndpointCandidate
    {
        std::wstring endpointDeviceId{};
        std::wstring name{};
        std::wstring transportSuppliedName{};
        uint16_t usbVendorId{ 0 };
        uint16_t usbProductId{ 0 };
        bool isLoopback{ false };
        std::array<bool, 16> sourceGroups{};
        std::array<bool, 16> destinationGroups{};
        std::array<std::wstring, 16> sourcePortNames{};
        std::array<std::wstring, 16> destinationPortNames{};
        std::array<std::wstring, 16> sourceGroupNames{};
        std::array<std::wstring, 16> destinationGroupNames{};
    };

    enum class LaunchpadHandshakeMode : uint8_t
    {
        Immediate,
        ConfirmWithDeviceInquiry,
    };

    struct LaunchpadSelection
    {
        std::wstring endpointDeviceId{};
        std::wstring displayName{};
        uint8_t group{ 0 };
        LaunchpadHandshakeMode handshake{ LaunchpadHandshakeMode::Immediate };
        bool explicitMatch{ false };

        bool operator==(LaunchpadSelection const&) const = default;
    };

    namespace detail
    {
        [[nodiscard]] inline constexpr wchar_t ToLowerAscii(_In_ wchar_t ch) noexcept
        {
            return (ch >= L'A' && ch <= L'Z') ? static_cast<wchar_t>(ch + (L'a' - L'A')) : ch;
        }

        [[nodiscard]] inline bool ContainsCaseInsensitive(_In_ std::wstring_view haystack, _In_ std::wstring_view needle) noexcept
        {
            if (needle.empty() || haystack.size() < needle.size())
            {
                return false;
            }

            for (size_t i = 0; i <= haystack.size() - needle.size(); ++i)
            {
                bool match = true;
                for (size_t j = 0; j < needle.size(); ++j)
                {
                    if (ToLowerAscii(haystack[i + j]) != ToLowerAscii(needle[j]))
                    {
                        match = false;
                        break;
                    }
                }

                if (match)
                {
                    return true;
                }
            }

            return false;
        }

        [[nodiscard]] inline bool EqualsCaseInsensitive(_In_ std::wstring_view left, _In_ std::wstring_view right) noexcept
        {
            if (left.size() != right.size())
            {
                return false;
            }

            for (size_t i = 0; i < left.size(); ++i)
            {
                if (ToLowerAscii(left[i]) != ToLowerAscii(right[i]))
                {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] inline bool IsDawPortName(_In_ std::wstring_view name) noexcept
        {
            return ContainsCaseInsensitive(name, L"lpx daw") ||
                ContainsCaseInsensitive(name, L"launchpad x daw");
        }

        [[nodiscard]] inline bool IsLpxMidiName(_In_ std::wstring_view name) noexcept
        {
            return !name.empty() && !IsDawPortName(name) && ContainsCaseInsensitive(name, L"lpx midi");
        }

        [[nodiscard]] inline bool IsLaunchpadNonDawName(_In_ std::wstring_view name) noexcept
        {
            if (name.empty() || IsDawPortName(name))
            {
                return false;
            }

            return IsLaunchpadMidiEndpointName(name);
        }

        [[nodiscard]] inline bool GroupHasDawName(_In_ LaunchpadEndpointCandidate const& candidate, _In_ size_t group) noexcept
        {
            return IsDawPortName(candidate.sourcePortNames[group]) ||
                IsDawPortName(candidate.destinationPortNames[group]) ||
                IsDawPortName(candidate.sourceGroupNames[group]) ||
                IsDawPortName(candidate.destinationGroupNames[group]);
        }

        [[nodiscard]] inline bool GroupHasLpxMidiName(_In_ LaunchpadEndpointCandidate const& candidate, _In_ size_t group) noexcept
        {
            if (GroupHasDawName(candidate, group))
            {
                return false;
            }

            return IsLpxMidiName(candidate.sourcePortNames[group]) ||
                IsLpxMidiName(candidate.destinationPortNames[group]) ||
                IsLpxMidiName(candidate.sourceGroupNames[group]) ||
                IsLpxMidiName(candidate.destinationGroupNames[group]);
        }

        [[nodiscard]] inline bool GroupHasLaunchpadName(_In_ LaunchpadEndpointCandidate const& candidate, _In_ size_t group) noexcept
        {
            if (GroupHasDawName(candidate, group))
            {
                return false;
            }

            return IsLaunchpadNonDawName(candidate.sourcePortNames[group]) ||
                IsLaunchpadNonDawName(candidate.destinationPortNames[group]) ||
                IsLaunchpadNonDawName(candidate.sourceGroupNames[group]) ||
                IsLaunchpadNonDawName(candidate.destinationGroupNames[group]);
        }

        [[nodiscard]] inline std::optional<uint8_t> ResolveLaunchpadGroup(
            _In_ LaunchpadEndpointCandidate const& candidate,
            _In_ bool allowUnknownGroupName,
            _Out_ bool& outMatchedLpxMidiGroup,
            _Out_ bool& outMatchedLaunchpadGroup) noexcept
        {
            outMatchedLpxMidiGroup = false;
            outMatchedLaunchpadGroup = false;

            bool anyDestinationGroup = false;
            for (bool const enabled : candidate.destinationGroups)
            {
                if (enabled)
                {
                    anyDestinationGroup = true;
                    break;
                }
            }

            auto const groupEligible = [&](size_t g) noexcept -> bool
            {
                if (anyDestinationGroup)
                {
                    return candidate.destinationGroups[g];
                }

                return true;
            };

            auto const tryPass = [&](auto&& predicate, bool requireBidirectional) noexcept -> std::optional<uint8_t>
            {
                for (size_t g = 0; g < 16; ++g)
                {
                    if (!groupEligible(g))
                    {
                        continue;
                    }

                    if (requireBidirectional && !(candidate.sourceGroups[g] && candidate.destinationGroups[g]))
                    {
                        continue;
                    }

                    if (predicate(g))
                    {
                        return static_cast<uint8_t>(g);
                    }
                }

                return std::nullopt;
            };

            // Priority 1: explicit "LPX MIDI" group/port name (not DAW)
            if (auto g = tryPass([&](size_t idx) noexcept { return GroupHasLpxMidiName(candidate, idx); }, true))
            {
                outMatchedLpxMidiGroup = true;
                outMatchedLaunchpadGroup = true;
                return g;
            }
            if (auto g = tryPass([&](size_t idx) noexcept { return GroupHasLpxMidiName(candidate, idx); }, false))
            {
                outMatchedLpxMidiGroup = true;
                outMatchedLaunchpadGroup = true;
                return g;
            }

            // Priority 2: "Launchpad X" non-DAW group/port name
            if (auto g = tryPass([&](size_t idx) noexcept { return GroupHasLaunchpadName(candidate, idx); }, true))
            {
                outMatchedLaunchpadGroup = true;
                return g;
            }
            if (auto g = tryPass([&](size_t idx) noexcept { return GroupHasLaunchpadName(candidate, idx); }, false))
            {
                outMatchedLaunchpadGroup = true;
                return g;
            }

            if (!allowUnknownGroupName)
            {
                return std::nullopt;
            }

            // Priority 3: non-DAW active group
            if (auto g = tryPass([&](size_t idx) noexcept { return !GroupHasDawName(candidate, idx); }, true))
            {
                return g;
            }
            if (auto g = tryPass([&](size_t idx) noexcept { return !GroupHasDawName(candidate, idx); }, false))
            {
                return g;
            }

            return std::nullopt;
        }

        [[nodiscard]] inline std::wstring ChooseDisplayName(_In_ LaunchpadEndpointCandidate const& candidate)
        {
            if (!candidate.name.empty())
            {
                return candidate.name;
            }

            if (!candidate.transportSuppliedName.empty())
            {
                return candidate.transportSuppliedName;
            }

            return candidate.endpointDeviceId;
        }
    }

    [[nodiscard]] inline std::optional<LaunchpadSelection> SelectLaunchpadEndpoint(
        _In_ std::span<const LaunchpadEndpointCandidate> candidates,
        _In_ std::wstring_view preferredEndpointId = {},
        _In_ std::wstring_view preferredEndpointName = {}) noexcept
    {
        try
        {
            bool const hasPreferred = !preferredEndpointId.empty() || !preferredEndpointName.empty();

            if (hasPreferred)
            {
                LaunchpadEndpointCandidate const* matched = nullptr;

                if (!preferredEndpointId.empty())
                {
                    for (auto const& candidate : candidates)
                    {
                        if (!candidate.endpointDeviceId.empty() &&
                            detail::EqualsCaseInsensitive(candidate.endpointDeviceId, preferredEndpointId))
                        {
                            matched = &candidate;
                            break;
                        }
                    }
                }

                if (matched == nullptr && !preferredEndpointName.empty())
                {
                    for (auto const& candidate : candidates)
                    {
                        if ((!candidate.name.empty() && detail::EqualsCaseInsensitive(candidate.name, preferredEndpointName)) ||
                            (!candidate.transportSuppliedName.empty() && detail::EqualsCaseInsensitive(candidate.transportSuppliedName, preferredEndpointName)))
                        {
                            matched = &candidate;
                            break;
                        }
                    }
                }

                if (matched == nullptr)
                {
                    return std::nullopt;
                }

                bool matchedLpxMidi = false;
                bool matchedLaunchpad = false;
                auto group = detail::ResolveLaunchpadGroup(*matched, true, matchedLpxMidi, matchedLaunchpad);

                return LaunchpadSelection{
                    matched->endpointDeviceId,
                    detail::ChooseDisplayName(*matched),
                    group.value_or(0),
                    LaunchpadHandshakeMode::Immediate,
                    true,
                };
            }

            int bestScore = -1;
            std::optional<LaunchpadSelection> best{};

            for (auto const& candidate : candidates)
            {
                if (candidate.endpointDeviceId.empty())
                {
                    continue;
                }

                bool matchedLpxMidiGroup = false;
                bool matchedLaunchpadGroup = false;
                auto const group = detail::ResolveLaunchpadGroup(candidate, true, matchedLpxMidiGroup, matchedLaunchpadGroup);

                if (!group.has_value())
                {
                    continue;
                }

                bool const endpointIsDawOnly =
                    (detail::IsDawPortName(candidate.name) || detail::IsDawPortName(candidate.transportSuppliedName)) &&
                    !matchedLpxMidiGroup;

                if (endpointIsDawOnly)
                {
                    continue;
                }

                bool const endpointLpxMidi =
                    detail::IsLpxMidiName(candidate.name) ||
                    detail::IsLpxMidiName(candidate.transportSuppliedName);

                bool const endpointLaunchpad =
                    detail::IsLaunchpadNonDawName(candidate.name) ||
                    detail::IsLaunchpadNonDawName(candidate.transportSuppliedName);

                int tierScore = 0;
                LaunchpadHandshakeMode handshake = LaunchpadHandshakeMode::Immediate;

                if (matchedLpxMidiGroup || endpointLpxMidi)
                {
                    tierScore = 300;
                    handshake = LaunchpadHandshakeMode::Immediate;
                }
                else if (matchedLaunchpadGroup || endpointLaunchpad)
                {
                    tierScore = 200;
                    handshake = LaunchpadHandshakeMode::Immediate;
                }
                else if (candidate.usbVendorId == NovationUsbVendorId)
                {
                    tierScore = 100;
                    handshake = LaunchpadHandshakeMode::ConfirmWithDeviceInquiry;
                }
                else
                {
                    continue;
                }

                if (candidate.usbVendorId == NovationUsbVendorId)
                {
                    tierScore += 20;
                }

                if (!candidate.isLoopback)
                {
                    tierScore += 10;
                }

                if (tierScore > bestScore)
                {
                    bestScore = tierScore;
                    best = LaunchpadSelection{
                        candidate.endpointDeviceId,
                        detail::ChooseDisplayName(candidate),
                        *group,
                        handshake,
                        false,
                    };
                }
            }

            return best;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    class LaunchpadXController
    {
    public:
        bool Connect(
            _In_ std::shared_ptr<ILaunchpadOutput> output,
            _In_ uint8_t group,
            _In_ SurfaceStateSnapshot const& initialSnapshot,
            _In_ LaunchpadHandshakeMode handshake = LaunchpadHandshakeMode::Immediate,
            _In_ bool sendDeviceInquiry = true) noexcept
        {
            try
            {
                if (output == nullptr)
                {
                    return false;
                }

                uint8_t const maskedGroup = static_cast<uint8_t>(group & 0x0Fu);
                std::vector<LedSpec> initialLeds{};

                {
                    std::scoped_lock guard{ m_lock };
                    m_sysexAccumulator.Reset();
                    m_output = output;
                    m_group = maskedGroup;
                    m_connected = true;
                    m_inquiryConfirmed = false;
                    m_isBootloader = false;
                    m_pendingFullRefresh = false;
                    m_firmwareVersion = {};

                    if (handshake == LaunchpadHandshakeMode::Immediate)
                    {
                        m_awaitingInquiry = false;
                        m_programmerModeActive = true;
                        initialLeds = m_surface.BuildLedFeedback(initialSnapshot, true);
                    }
                    else
                    {
                        m_awaitingInquiry = true;
                        m_programmerModeActive = false;
                    }
                }

                std::vector<UmpPacket> packets{};

                if (sendDeviceInquiry || handshake == LaunchpadHandshakeMode::ConfirmWithDeviceInquiry)
                {
                    auto inquiry = BuildDeviceInquiryPackets(maskedGroup);
                    packets.insert(packets.end(), inquiry.begin(), inquiry.end());
                }

                if (handshake == LaunchpadHandshakeMode::Immediate)
                {
                    auto enter = BuildProgrammerModePackets(maskedGroup, true);
                    packets.insert(packets.end(), enter.begin(), enter.end());

                    if (!initialLeds.empty())
                    {
                        auto ledPackets = BuildLedSysExPackets(maskedGroup, initialLeds);
                        packets.insert(packets.end(), ledPackets.begin(), ledPackets.end());
                    }
                }

                if (!packets.empty() && !output->SendUmpPackets(packets))
                {
                    std::scoped_lock guard{ m_lock };
                    if (m_output == output)
                    {
                        m_connected = false;
                        m_awaitingInquiry = false;
                        m_programmerModeActive = false;
                        m_output.reset();
                    }

                    return false;
                }

                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        void Disconnect(
            _In_ bool sendExitProgrammerMode,
            _Out_ std::vector<SurfaceAction>& outReleaseActions) noexcept
        {
            outReleaseActions.clear();

            try
            {
                std::shared_ptr<ILaunchpadOutput> output{};
                bool wasActive = false;
                uint8_t group = 0;

                {
                    std::scoped_lock guard{ m_lock };
                    if (m_surface.Mode() == ControllerMode::Note)
                    {
                        std::vector<SurfaceAction> dummy{};
                        m_surface.SetMode(ControllerMode::Clip, outReleaseActions);
                        m_surface.SetMode(ControllerMode::Note, dummy);
                    }

                    output = std::exchange(m_output, nullptr);
                    wasActive = m_connected && m_programmerModeActive && !m_isBootloader;
                    group = m_group;

                    m_connected = false;
                    m_awaitingInquiry = false;
                    m_programmerModeActive = false;
                    m_inquiryConfirmed = false;
                    m_isBootloader = false;
                    m_pendingFullRefresh = false;
                    m_firmwareVersion = {};
                    m_sysexAccumulator.Reset();
                }

                if (sendExitProgrammerMode && wasActive && output != nullptr)
                {
                    auto const exitPackets = BuildProgrammerModePackets(group, false);
                    (void)output->SendUmpPackets(exitPackets);
                }
            }
            catch (...)
            {
            }
        }

        bool OnUmpMessage(
            _In_reads_opt_(wordCount) uint32_t const* words,
            _In_ uint8_t wordCount,
            _In_ SurfaceStateSnapshot const& snapshot,
            _Out_ std::vector<SurfaceAction>& outActions) noexcept
        {
            outActions.clear();

            try
            {
                if (words == nullptr || wordCount == 0)
                {
                    return false;
                }

                std::shared_ptr<ILaunchpadOutput> output{};
                uint8_t group = 0;
                bool sendEnterAndLeds = false;
                std::vector<LedSpec> initialLeds{};

                {
                    std::scoped_lock guard{ m_lock };

                    if (!m_connected)
                    {
                        return false;
                    }

                    uint8_t const msgGroup = static_cast<uint8_t>((words[0] >> 24) & 0x0Fu);
                    if (msgGroup != m_group)
                    {
                        return false;
                    }

                    InputEvent event{};
                    if (!TryParseInputUmp(words, wordCount, event, &m_sysexAccumulator))
                    {
                        return false;
                    }

                    if (event.kind == InputEventKind::DeviceInquiryReply)
                    {
                        m_firmwareVersion = {
                            event.version[0],
                            event.version[1],
                            event.version[2],
                            event.version[3],
                        };
                        m_isBootloader = event.isBootloader;
                        m_inquiryConfirmed = !event.isBootloader;

                        if (m_awaitingInquiry)
                        {
                            m_awaitingInquiry = false;

                            if (!event.isBootloader)
                            {
                                m_programmerModeActive = true;
                                m_pendingFullRefresh = false;
                                initialLeds = m_surface.BuildLedFeedback(snapshot, true);
                                output = m_output;
                                group = m_group;
                                sendEnterAndLeds = true;
                            }
                            else
                            {
                                m_programmerModeActive = false;
                            }
                        }
                        else if (event.isBootloader)
                        {
                            m_programmerModeActive = false;
                        }
                    }
                    else
                    {
                        if (!m_programmerModeActive || m_isBootloader)
                        {
                            return false;
                        }

                        m_surface.HandleInput(event, snapshot, outActions);
                        return true;
                    }
                }

                if (sendEnterAndLeds && output != nullptr)
                {
                    auto packets = BuildProgrammerModePackets(group, true);

                    if (!initialLeds.empty())
                    {
                        auto ledPackets = BuildLedSysExPackets(group, initialLeds);
                        packets.insert(packets.end(), ledPackets.begin(), ledPackets.end());
                    }

                    if (!output->SendUmpPackets(packets))
                    {
                        std::scoped_lock guard{ m_lock };
                        if (m_connected && m_output == output)
                        {
                            m_pendingFullRefresh = true;
                        }
                    }
                }

                return true;
            }
            catch (...)
            {
                outActions.clear();
                return false;
            }
        }

        bool SyncLeds(_In_ SurfaceStateSnapshot const& snapshot, _In_ bool forceFullRefresh = false) noexcept
        {
            try
            {
                std::shared_ptr<ILaunchpadOutput> output{};
                std::vector<LedSpec> leds{};
                uint8_t group = 0;

                {
                    std::scoped_lock guard{ m_lock };

                    if (!m_connected || !m_programmerModeActive || m_isBootloader || m_output == nullptr)
                    {
                        return false;
                    }

                    bool const full = forceFullRefresh || m_pendingFullRefresh;
                    m_pendingFullRefresh = false;
                    leds = m_surface.BuildLedFeedback(snapshot, full);
                    output = m_output;
                    group = m_group;
                }

                if (leds.empty())
                {
                    return true;
                }

                auto const packets = BuildLedSysExPackets(group, leds);
                if (!output->SendUmpPackets(packets))
                {
                    std::scoped_lock guard{ m_lock };
                    if (m_connected && m_output == output)
                    {
                        m_pendingFullRefresh = true;
                    }

                    return false;
                }

                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        void SetMode(_In_ ControllerMode mode, _Out_ std::vector<SurfaceAction>& outReleaseActions) noexcept
        {
            outReleaseActions.clear();

            try
            {
                std::scoped_lock guard{ m_lock };
                m_surface.SetMode(mode, outReleaseActions);
            }
            catch (...)
            {
            }
        }

        void SetNoteConfig(_In_ NoteLayoutConfig const& config, _Out_ std::vector<SurfaceAction>& outReleaseActions) noexcept
        {
            outReleaseActions.clear();

            try
            {
                std::scoped_lock guard{ m_lock };
                m_surface.SetNoteConfig(config, outReleaseActions);
            }
            catch (...)
            {
            }
        }

        void SetViewport(_In_ uint32_t trackOffset, _In_ uint32_t sceneOffset) noexcept
        {
            std::scoped_lock guard{ m_lock };
            m_surface.SetViewport(trackOffset, sceneOffset);
        }

        [[nodiscard]] ControllerMode Mode() const noexcept
        {
            std::scoped_lock guard{ m_lock };
            return m_surface.Mode();
        }

        [[nodiscard]] NoteLayoutConfig NoteConfig() const noexcept
        {
            std::scoped_lock guard{ m_lock };
            return m_surface.NoteConfig();
        }

        [[nodiscard]] uint32_t TrackOffset() const noexcept
        {
            std::scoped_lock guard{ m_lock };
            return m_surface.TrackOffset();
        }

        [[nodiscard]] uint32_t SceneOffset() const noexcept
        {
            std::scoped_lock guard{ m_lock };
            return m_surface.SceneOffset();
        }

        [[nodiscard]] bool IsConnected() const noexcept
        {
            std::scoped_lock guard{ m_lock };
            return m_connected;
        }

        [[nodiscard]] bool IsProgrammerModeActive() const noexcept
        {
            std::scoped_lock guard{ m_lock };
            return m_programmerModeActive;
        }

        [[nodiscard]] bool IsInquiryConfirmed() const noexcept
        {
            std::scoped_lock guard{ m_lock };
            return m_inquiryConfirmed;
        }

        [[nodiscard]] bool IsBootloader() const noexcept
        {
            std::scoped_lock guard{ m_lock };
            return m_isBootloader;
        }

        [[nodiscard]] uint8_t Group() const noexcept
        {
            std::scoped_lock guard{ m_lock };
            return m_group;
        }

        [[nodiscard]] std::array<uint8_t, 4> FirmwareVersion() const noexcept
        {
            std::scoped_lock guard{ m_lock };
            return m_firmwareVersion;
        }

    private:
        mutable std::mutex m_lock{};
        std::shared_ptr<ILaunchpadOutput> m_output{};
        LaunchpadXSurface m_surface{};
        SysEx7Accumulator m_sysexAccumulator{};
        uint8_t m_group{ 0 };
        bool m_connected{ false };
        bool m_awaitingInquiry{ false };
        bool m_programmerModeActive{ false };
        bool m_inquiryConfirmed{ false };
        bool m_isBootloader{ false };
        bool m_pendingFullRefresh{ false };
        std::array<uint8_t, 4> m_firmwareVersion{};
    };
}

#ifdef WINRT_Windows_Devices_Midi2_H
namespace midiapp
{
    struct LiveEndpoint;
}

namespace midisequencer::launchpad
{
    class LaunchpadXSessionTransport final :
        public ILaunchpadOutput,
        public std::enable_shared_from_this<LaunchpadXSessionTransport>
    {
    public:
        using OnUmpCallback = std::function<void(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount, _In_ uint64_t timestamp)>;
        using OnStateCallback = std::function<void(_In_ bool reconnected)>;

        explicit LaunchpadXSessionTransport(_In_ ::winrt::Windows::Devices::Midi2::MidiSession const& session) noexcept;
        ~LaunchpadXSessionTransport() override;

        LaunchpadXSessionTransport(LaunchpadXSessionTransport const&) = delete;
        LaunchpadXSessionTransport& operator=(LaunchpadXSessionTransport const&) = delete;

        [[nodiscard]] static LaunchpadEndpointCandidate BuildCandidate(
            _In_ midiapp::LiveEndpoint const& live,
            _In_ uint16_t sourceGroupsOffMask = 0,
            _In_ uint16_t destinationGroupsOffMask = 0) noexcept;

        // Opens the live endpoint connection on the service. Blocks on RPC; never call on UI thread.
        bool Open(
            _In_ std::wstring const& endpointDeviceId,
            _In_ uint8_t group,
            _In_ OnUmpCallback onUmp,
            _In_ OnStateCallback onState) noexcept;

        void Close() noexcept;

        bool SendUmpPackets(_In_ std::span<const UmpPacket> packets) noexcept override;

        [[nodiscard]] std::wstring EndpointDeviceId() const;
        [[nodiscard]] uint8_t Group() const noexcept;

    private:
        void DeliverMessage(_In_ ::winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs const& args) noexcept;
        void DeliverState(_In_ bool reconnected) noexcept;

        ::winrt::Windows::Devices::Midi2::MidiSession m_session{ nullptr };
        mutable std::mutex m_lock{};
        ::winrt::Windows::Devices::Midi2::MidiEndpointConnection m_connection{ nullptr };
        ::winrt::event_token m_messageToken{};
        ::winrt::event_token m_disconnectedToken{};
        ::winrt::event_token m_reconnectedToken{};
        uint32_t m_maxWordsPerTransmission{ 684 };
        std::wstring m_endpointDeviceId{};
        uint8_t m_group{ 0 };
        OnUmpCallback m_onUmp{};
        OnStateCallback m_onState{};
    };
}
#endif
