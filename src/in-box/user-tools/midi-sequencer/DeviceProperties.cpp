// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "DeviceProperties.h"

#include "MidiDefs.h"

#include <winrt/Windows.Devices.Midi2.Reporting.h>

namespace midisequencer
{
    namespace
    {
        namespace midi2rept = ::winrt::Windows::Devices::Midi2::Reporting;

        // The speed of a MIDI 1.0 cable, which a group terminal block reports for one.
        constexpr uint32_t CableBitsPerSecond = 31250;

        std::wstring Lowered(std::wstring text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](wchar_t const ch) { return static_cast<wchar_t>(::towlower(ch)); });
            return text;
        }

        bool BoolProperty(collections::IMapView<winrt::hstring, foundation::IInspectable> const& properties, wchar_t const* key) noexcept
        {
            try
            {
                return properties != nullptr && winrt::unbox_value_or<bool>(properties.TryLookup(key), false);
            }
            catch (...)
            {
                return false;
            }
        }

        // Block input and block output are named from the device's side, so a block the device
        // takes input on is one the sequencer plays to.
        BlockDirection FromFunctionBlock(midi2enum::MidiFunctionBlockDirection const direction) noexcept
        {
            switch (direction)
            {
            case midi2enum::MidiFunctionBlockDirection::BlockInput: return BlockDirection::Destination;
            case midi2enum::MidiFunctionBlockDirection::BlockOutput: return BlockDirection::Source;
            default: return BlockDirection::Both;
            }
        }

        BlockDirection FromGroupTerminalBlock(midi2enum::MidiGroupTerminalBlockDirection const direction) noexcept
        {
            switch (direction)
            {
            case midi2enum::MidiGroupTerminalBlockDirection::BlockInput: return BlockDirection::Destination;
            case midi2enum::MidiGroupTerminalBlockDirection::BlockOutput: return BlockDirection::Source;
            default: return BlockDirection::Both;
            }
        }

        // The same rules the engine uses to choose MIDI 1.0 or MIDI 2.0 for a group, so what the
        // details say is what's sent.
        void ReadBlocks(midi2enum::MidiEndpointDeviceInformation const& device, bool const endpointMidi1, DeviceProperties& result)
        {
            auto const blocks = device.GetDeclaredFunctionBlocks();

            if (blocks != nullptr && blocks.Size() > 0)
            {
                result.FunctionBlocks = true;

                for (auto const& block : blocks)
                {
                    DeviceBlock entry{};
                    entry.Name = std::wstring{ block.Name() };
                    entry.FirstGroup = block.FirstGroup().Index();
                    entry.GroupCount = block.GroupCount();
                    entry.Direction = FromFunctionBlock(block.Direction());
                    entry.Active = block.IsActive();

                    // Anything other than "not MIDI 1.0" is MIDI 1.0 to the engine.
                    switch (block.RepresentsMidi10Connection())
                    {
                    case midi2enum::MidiFunctionBlockRepresentsMidi10Connection::YesBandwidthRestricted:
                        entry.Protocol = BlockProtocol::Midi1CableSpeed;
                        break;
                    case midi2enum::MidiFunctionBlockRepresentsMidi10Connection::Not10:
                        entry.Protocol = endpointMidi1 ? BlockProtocol::Midi1 : BlockProtocol::Midi2;
                        break;
                    default:
                        entry.Protocol = BlockProtocol::Midi1;
                        break;
                    }

                    result.Blocks.push_back(std::move(entry));
                }

                return;
            }

            auto const terminals = device.GetGroupTerminalBlocks();

            if (terminals == nullptr)
            {
                return;
            }

            for (auto const& terminal : terminals)
            {
                DeviceBlock entry{};
                entry.Name = std::wstring{ terminal.Name() };
                entry.FirstGroup = terminal.FirstGroup().Index();
                entry.GroupCount = terminal.GroupCount();
                entry.Direction = FromGroupTerminalBlock(terminal.Direction());

                switch (terminal.Protocol())
                {
                case midi2enum::MidiGroupTerminalBlockProtocol::Midi1Message64:
                case midi2enum::MidiGroupTerminalBlockProtocol::Midi1Message64WithJitterReduction:
                case midi2enum::MidiGroupTerminalBlockProtocol::Midi1Message128:
                case midi2enum::MidiGroupTerminalBlockProtocol::Midi1Message128WithJitterReduction:
                {
                    // The speed that matters is the one toward wherever the messages go.
                    auto const bandwidth = entry.Direction == BlockDirection::Source
                        ? terminal.CalculatedMaxDeviceOutputBandwidthBitsPerSecond()
                        : terminal.CalculatedMaxDeviceInputBandwidthBitsPerSecond();

                    entry.Protocol = bandwidth > 0 && bandwidth <= CableBitsPerSecond ? BlockProtocol::Midi1CableSpeed : BlockProtocol::Midi1;
                    break;
                }
                default:
                    entry.Protocol = endpointMidi1 ? BlockProtocol::Midi1 : BlockProtocol::Midi2;
                    break;
                }

                result.Blocks.push_back(std::move(entry));
            }
        }

        void ReadIdentity(midi2enum::MidiEndpointDeviceInformation const& device, DeviceProperties& result)
        {
            auto const identity = device.GetDeclaredDeviceIdentity();

            if (identity == nullptr)
            {
                return;
            }

            auto const id = identity.SystemExclusiveId();
            auto const revision = identity.SoftwareRevisionLevel();

            for (size_t i = 0; i < result.SystemExclusiveId.size() && i < id.size(); ++i)
            {
                result.SystemExclusiveId[i] = id[static_cast<uint32_t>(i)];
            }

            for (size_t i = 0; i < result.SoftwareRevision.size() && i < revision.size(); ++i)
            {
                result.SoftwareRevision[i] = revision[static_cast<uint32_t>(i)];
            }

            result.Family = static_cast<uint16_t>((identity.DeviceFamilyMsb() << 7) | (identity.DeviceFamilyLsb() & 0x7F));
            result.Model = static_cast<uint16_t>((identity.DeviceFamilyModelNumberMsb() << 7) | (identity.DeviceFamilyModelNumberLsb() & 0x7F));

            auto const any = [](auto const& bytes)
            {
                return std::any_of(bytes.begin(), bytes.end(), [](uint8_t const value) { return value != 0; });
            };

            // A device that never answered has an identity of all zeros.
            result.HasIdentity = any(result.SystemExclusiveId) || any(result.SoftwareRevision) || result.Family != 0 || result.Model != 0;
        }
    }

    _Use_decl_annotations_
    DeviceOffset ReadDeviceOffset(midi2enum::MidiEndpointDeviceInformation const& device) noexcept
    {
        constexpr uint64_t MaximumMicroseconds = 1000000;

        DeviceOffset offset{};

        try
        {
            auto const properties = device.Properties();

            if (properties == nullptr)
            {
                return offset;
            }

            auto const calculated = winrt::unbox_value_or<uint64_t>(properties.TryLookup(STRING_PKEY_MIDI_MidiOutCalculatedLatencyTicks), 0);
            auto const custom = winrt::unbox_value_or<uint64_t>(properties.TryLookup(STRING_PKEY_MIDI_MidiOutCustomLatencyTicks), 0);

            // A service that doesn't record the choice treats a custom value as the choice.
            auto const choice = properties.TryLookup(STRING_PKEY_MIDI_MidiOutLatencyTicksUserOverride);
            auto const useCustom = choice != nullptr ? winrt::unbox_value_or<bool>(choice, false) : custom != 0;

            offset.ChosenInMidiSettings = useCustom;

            // Stored unsigned but meant signed. A negative offset isn't applied.
            auto const ticks = static_cast<int64_t>(useCustom ? custom : calculated);
            auto const frequency = midi2::MidiClock::TimestampFrequency();

            if (ticks <= 0 || frequency == 0)
            {
                return offset;
            }

            if (static_cast<uint64_t>(ticks) >= frequency)
            {
                offset.Microseconds = static_cast<uint32_t>(MaximumMicroseconds);
                return offset;
            }

            offset.Microseconds = static_cast<uint32_t>(static_cast<uint64_t>(ticks) * MaximumMicroseconds / frequency);
        }
        catch (...)
        {
            offset.Microseconds = 0;
        }

        return offset;
    }

    _Use_decl_annotations_
    uint32_t ReadAutomationInterval(midi2enum::MidiEndpointDeviceInformation const& device) noexcept
    {
        try
        {
            auto const properties = device.Properties();
            auto const value = properties != nullptr ? properties.TryLookup(STRING_PKEY_MIDI_RecommendedCCAutomationIntervalMS) : nullptr;

            if (value == nullptr)
            {
                return 0;
            }

            // Stored as a number of more than one width, depending on what wrote it.
            if (auto const number = value.try_as<foundation::IReference<int32_t>>(); number != nullptr)
            {
                return static_cast<uint32_t>(std::clamp(number.Value(), 0, 1000));
            }

            if (auto const number = value.try_as<foundation::IReference<uint32_t>>(); number != nullptr)
            {
                return std::min<uint32_t>(number.Value(), 1000);
            }

            if (auto const number = value.try_as<foundation::IReference<uint16_t>>(); number != nullptr)
            {
                return std::min<uint32_t>(number.Value(), 1000);
            }
        }
        catch (...)
        {
        }

        return 0;
    }

    _Use_decl_annotations_
    std::wstring TransportDisplayName(std::wstring const& transportCode) noexcept
    {
        static std::mutex lock{};
        static std::unordered_map<std::wstring, std::wstring> names{};
        static bool loaded{ false };

        try
        {
            if (transportCode.empty())
            {
                return {};
            }

            bool load{ false };

            {
                std::scoped_lock guard{ lock };
                load = !loaded;
            }

            // Asked for once, and outside the lock, because it goes to the service.
            if (load)
            {
                std::unordered_map<std::wstring, std::wstring> fetched{};

                if (auto const installed = midi2rept::MidiReporting::GetInstalledTransportPlugins(); installed != nullptr)
                {
                    for (auto const& transport : installed)
                    {
                        auto code = Lowered(std::wstring{ transport.TransportCode() });
                        std::wstring name{ transport.Name() };

                        if (!code.empty() && !name.empty())
                        {
                            fetched.insert_or_assign(std::move(code), std::move(name));
                        }
                    }
                }

                // An empty answer means the service wasn't there, so the next caller asks again.
                if (!fetched.empty())
                {
                    std::scoped_lock guard{ lock };

                    if (!loaded)
                    {
                        names = std::move(fetched);
                        loaded = true;
                    }
                }
            }

            std::scoped_lock guard{ lock };

            if (auto const found = names.find(Lowered(transportCode)); found != names.end())
            {
                return found->second;
            }
        }
        catch (...)
        {
        }

        return transportCode;
    }

    _Use_decl_annotations_
    DeviceProperties ReadDeviceProperties(std::wstring const& endpointDeviceId) noexcept
    {
        DeviceProperties result{};
        result.Id = endpointDeviceId;

        if (endpointDeviceId.empty())
        {
            return result;
        }

        try
        {
            auto const device = midi2enum::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(endpointDeviceId);

            if (device == nullptr)
            {
                return result;
            }

            result.Found = true;
            result.Name = std::wstring{ device.Name() };

            // Each part on its own, so one the device doesn't have leaves the rest.
            try
            {
                if (auto const transport = device.GetTransportSuppliedInfo(); transport != nullptr)
                {
                    result.TransportCode = std::wstring{ transport.TransportCode() };
                    result.Manufacturer = std::wstring{ transport.ManufacturerName() };
                    result.SerialNumber = std::wstring{ transport.SerialNumber() };
                    result.VendorId = transport.VendorId();
                    result.ProductId = transport.ProductId();
                    result.ByteStream = transport.NativeDataFormat() == midi2enum::MidiEndpointNativeDataFormat::Midi1ByteFormat;
                    result.Description = std::wstring{ transport.Description() };
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }

            std::wstring customPicture{};

            try
            {
                if (auto const user = device.GetUserSuppliedInfo(); user != nullptr)
                {
                    customPicture = std::wstring{ midiapp::ResolveEndpointImagePath(user.ImageFileName()) };

                    // The customer's own words win, because they're the ones reading them.
                    if (std::wstring described{ user.Description() }; !described.empty())
                    {
                        result.Description = std::move(described);
                    }
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }

            result.CustomPicture = !customPicture.empty();
            result.PicturePath = std::wstring{ midiapp::ResolveEndpointImageOrDefault(winrt::hstring{ customPicture }, result.TransportCode) };
            result.TransportName = TransportDisplayName(result.TransportCode);

            try
            {
                if (auto const parent = device.GetParentDeviceInformation(); parent != nullptr)
                {
                    result.ParentName = std::wstring{ parent.Name() };
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }

            try
            {
                if (auto const info = device.GetDeclaredEndpointInfo(); info != nullptr)
                {
                    result.ProductInstanceId = std::wstring{ info.ProductInstanceId() };
                    result.DeclaresMidi1 = info.SupportsMidi10Protocol();
                    result.DeclaresMidi2 = info.SupportsMidi20Protocol();
                    result.StaticFunctionBlocks = info.HasStaticFunctionBlocks();
                    result.UmpVersionMajor = info.SpecificationVersionMajor();
                    result.UmpVersionMinor = info.SpecificationVersionMinor();
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }

            // A MIDI 1.0 byte stream device gets MIDI 1.0 whatever else it says, and so does an
            // endpoint that settled on MIDI 1.0. Anything else gets MIDI 2.0, as in the engine.
            auto endpointMidi1 = result.ByteStream;

            try
            {
                if (auto const stream = device.GetDeclaredStreamConfiguration(); stream != nullptr && stream.Protocol() == midi2enum::MidiProtocol::Midi1)
                {
                    endpointMidi1 = true;
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }

            result.SpeaksMidi2 = !endpointMidi1;

            try
            {
                ReadBlocks(device, endpointMidi1, result);
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }

            try
            {
                ReadIdentity(device, result);
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }

            auto const properties = device.Properties();

            result.Offset = ReadDeviceOffset(device);
            result.AutomationIntervalMilliseconds = ReadAutomationInterval(device);
            result.SupportsMpe = BoolProperty(properties, STRING_PKEY_MIDI_SupportsMidiPolyphonicExpression);
            result.RequiresNoteOffTranslation = BoolProperty(properties, STRING_PKEY_MIDI_RequiresNoteOffTranslation);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return result;
    }
}
