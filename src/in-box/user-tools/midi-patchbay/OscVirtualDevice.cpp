// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "OscVirtualDevice.h"
#include "OscText.h"
#include "StringResources.h"

#include <winrt/Windows.Devices.Midi2.Transports.Virtual.h>

namespace midi2virtual = ::winrt::Windows::Devices::Midi2::Transports::Virtual;

namespace midipatchbay
{
    namespace
    {
        // Names past this are cut where a character ends, well inside what an endpoint or a
        // function block name can hold.
        constexpr size_t MaximumDeviceNameLength = 60;

        std::wstring Cut(_In_ std::wstring text)
        {
            if (text.size() > MaximumDeviceNameLength)
            {
                text.resize(MaximumDeviceNameLength);

                if (IS_HIGH_SURROGATE(text.back()))
                {
                    text.pop_back();
                }
            }

            return text;
        }

        std::wstring Lower(_In_ std::wstring text)
        {
            for (auto& c : text)
            {
                c = static_cast<wchar_t>(std::towlower(c));
            }

            return text;
        }

        // The id a new device asks for. Windows MIDI Services can't make a second device with an
        // id one already has, and taking a device away isn't over when the call returns. So each id is asked for at most once in this process, and never
        // while an endpoint has it: another copy of Windows MIDI Patchbay, say, or one left by a
        // device that never opened. The first device each run gets the plain id, which lets apps
        // find the same device from day to day. Any after it get "-2", "-3" and so on. Blocks
        // while the endpoints are listed.
        std::wstring FreshProductInstanceId(_In_ std::wstring const& id)
        {
            static std::mutex lock{};
            static std::map<std::wstring, uint32_t> asked{};

            std::vector<std::wstring> taken{};
            auto listed = false;

            try
            {
                auto const endpoints = midi2enum::MidiEndpointDeviceInformation::FindAll(
                    midi2enum::MidiEndpointDeviceInformationSortOrder::None,
                    midi2enum::MidiEndpointDeviceInformationFilters::AllStandardEndpoints |
                    midi2enum::MidiEndpointDeviceInformationFilters::VirtualDeviceResponder);

                for (auto const& endpoint : endpoints)
                {
                    taken.push_back(Lower(std::wstring{ endpoint.EndpointDeviceId() }));
                }

                listed = true;
            }
            catch (...)
            {
            }

            auto const inUse = [&taken](std::wstring const& candidate)
                {
                    auto const lower = Lower(candidate);

                    return std::any_of(taken.begin(), taken.end(), [&lower](std::wstring const& endpointId)
                        {
                            return endpointId.find(L"#midiu_appdev_" + lower + L"#") != std::wstring::npos ||
                                endpointId.find(L"#midiu_apppub_" + lower + L"#") != std::wstring::npos;
                        });
                };

            std::scoped_lock guard{ lock };
            auto& count = asked[id];

            // Unable to tell what's in use, this process's own id keeps it apart.
            if (!listed)
            {
                return std::format(L"{}-P{}-{}", id, ::GetCurrentProcessId(), ++count);
            }

            for (;;)
            {
                count++;

                auto candidate = count == 1 ? id : std::format(L"{}-{}", id, count);

                if (!inUse(candidate))
                {
                    return candidate;
                }
            }
        }

        uint8_t UmpWords(_In_ uint32_t firstWord) noexcept
        {
            switch (firstWord >> 28)
            {
            case 0x0: case 0x1: case 0x2: case 0x6: case 0x7: return 1;
            case 0x3: case 0x4: case 0x8: case 0x9: case 0xA: return 2;
            case 0xB: case 0xC: return 3;
            default: return 4;
            }
        }

        class Device final : public OscVirtualDevice
        {
        public:
            explicit Device(_In_ Handler handler) :
                m_handler{ std::move(handler) }
            {
            }

            ~Device() noexcept override
            {
                Close();
            }

            // False when it couldn't be made. madeEndpoint says whether the service made the
            // endpoint anyway, which nothing can take away until the service restarts.
            bool Open(_In_ OscDeviceShape const& shape, _Out_ bool& madeEndpoint)
            {
                madeEndpoint = false;

                if (!midi2virtual::MidiVirtualDeviceManager::IsTransportAvailable())
                {
                    return false;
                }

                m_session = midi2::MidiSession::Create(resources::GetString(L"OscDeviceSessionName"));

                if (m_session == nullptr)
                {
                    return false;
                }

                midi2enum::MidiDeclaredEndpointInfo info{};
                info.Name(winrt::hstring{ shape.Name });
                info.ProductInstanceId(winrt::hstring{ FreshProductInstanceId(shape.ProductInstanceId) });
                info.SupportsMidi10Protocol(true);
                info.SupportsMidi20Protocol(true);
                info.SupportsReceivingJitterReductionTimestamps(false);
                info.SupportsSendingJitterReductionTimestamps(false);
                info.SpecificationVersionMajor(1);
                info.SpecificationVersionMinor(1);
                info.HasStaticFunctionBlocks(true);

                midi2virtual::MidiVirtualDeviceCreationConfig config{ info.Name(), resources::GetString(L"OscDeviceDescription"), L"Microsoft", info };

                uint8_t number{ 0 };

                for (auto const& planned : shape.Blocks)
                {
                    midi2enum::MidiFunctionBlock block{};
                    block.Number(number++);
                    block.Name(winrt::hstring{ planned.Name });
                    block.IsActive(true);
                    block.Direction(midi2enum::MidiFunctionBlockDirection::Bidirectional);
                    block.UIHint(midi2enum::MidiFunctionBlockUIHint::Bidirectional);
                    block.RepresentsMidi10Connection(midi2enum::MidiFunctionBlockRepresentsMidi10Connection::Not10);
                    block.FirstGroup(midi2::MidiGroup{ planned.Group });
                    block.GroupCount(1);
                    config.FunctionBlocks().Append(block);
                }

                auto const device = midi2virtual::MidiVirtualDeviceManager::CreateVirtualDevice(config);

                if (device == nullptr)
                {
                    return false;
                }

                madeEndpoint = true;

                auto const connection = m_session.CreateEndpointConnection(device.DeviceEndpointDeviceId());

                if (connection == nullptr)
                {
                    return false;
                }

                auto const token = connection.MessageReceived([this](auto&&, midi2::MidiMessageReceivedEventArgs const& args)
                    {
                        Deliver(args);
                    });

                // The device has to be on the connection before it opens, or the endpoint other
                // apps see is never made.
                if (connection.AddMessageProcessingPlugin(device) != midi2::MidiMessageProcessingPluginAddResult::Succeeded || !connection.Open())
                {
                    connection.MessageReceived(token);
                    m_session.DisconnectEndpointConnection(connection.ConnectionId());
                    return false;
                }

                m_device = device;
                m_connection = connection;
                m_token = token;

                return true;
            }

            // Under the owner's dispatch lock, which Close never runs under at the same time.
            void Send(uint64_t timestamp, uint32_t const* words, uint32_t wordCount) noexcept override
            {
                if (m_connection == nullptr || words == nullptr)
                {
                    return;
                }

                try
                {
                    uint32_t position{ 0 };

                    while (position < wordCount)
                    {
                        auto const length = UmpWords(words[position]);

                        if (position + length > wordCount)
                        {
                            break;
                        }

                        auto const* const ump = words + position;

                        switch (length)
                        {
                        case 1: m_connection.SendSingleMessageWords(timestamp, ump[0]); break;
                        case 2: m_connection.SendSingleMessageWords(timestamp, ump[0], ump[1]); break;
                        case 3: m_connection.SendSingleMessageWords(timestamp, ump[0], ump[1], ump[2]); break;
                        default: m_connection.SendSingleMessageWords(timestamp, ump[0], ump[1], ump[2], ump[3]); break;
                        }

                        position += length;
                    }
                }
                catch (...)
                {
                }
            }

        private:
            // On the service's thread. Holds the lock while the handler runs, so Close waits for it.
            void Deliver(_In_ midi2::MidiMessageReceivedEventArgs const& args) noexcept
            {
                try
                {
                    uint32_t words[4]{};
                    auto const count = args.FillWords(words[0], words[1], words[2], words[3]);

                    if (count == 0 || count > 4)
                    {
                        return;
                    }

                    std::scoped_lock guard{ m_lock };

                    if (m_handler)
                    {
                        m_handler(args.Timestamp(), words, count);
                    }
                }
                catch (...)
                {
                    // The service's thread must never see an exception.
                }
            }

            void Close() noexcept
            {
                try
                {
                    std::scoped_lock guard{ m_lock };
                    m_handler = nullptr;
                }
                catch (...)
                {
                }

                try
                {
                    if (m_connection != nullptr)
                    {
                        m_connection.MessageReceived(m_token);

                        if (m_session != nullptr)
                        {
                            m_session.DisconnectEndpointConnection(m_connection.ConnectionId());
                        }
                    }

                    if (m_session != nullptr)
                    {
                        m_session.Close();
                    }
                }
                catch (...)
                {
                }

                m_connection = nullptr;
                m_device = nullptr;
                m_session = nullptr;
            }

            std::mutex m_lock{};
            Handler m_handler{};

            midi2::MidiSession m_session{ nullptr };
            midi2virtual::MidiVirtualDevice m_device{ nullptr };
            midi2::MidiEndpointConnection m_connection{ nullptr };
            winrt::event_token m_token{};
        };
    }

    _Use_decl_annotations_
    OscDeviceShape OscDeviceShapeOf(std::wstring const& deviceId, std::wstring const& name, OscConnectionSettings const& settings)
    {
        OscDeviceShape shape{};

        shape.Name = Cut(name.empty() ? std::wstring{ resources::GetString(L"OscDeviceDefaultName") } : name);
        shape.ProductInstanceId = OscDeviceProductInstanceId(deviceId);

        // A block for each row, named for its address, so an app shows which is which.
        for (auto const& row : settings.Rows)
        {
            shape.Blocks.push_back(OscDeviceBlock{ row.Group, Cut(OscRowLabel(row)) });
        }

        // A device needs a block. With no rows nothing passes, but it's still there to open.
        if (shape.Blocks.empty())
        {
            shape.Blocks.push_back(OscDeviceBlock{ 0, shape.Name });
        }

        return shape;
    }

    _Use_decl_annotations_
    std::unique_ptr<OscVirtualDevice> OscVirtualDevice::Create(OscDeviceShape const& shape, Handler handler, bool& tryAgain) noexcept
    {
        // Only when nothing was made, so failures don't leave endpoints behind one after another.
        tryAgain = true;

        try
        {
            auto device = std::make_unique<Device>(std::move(handler));
            auto madeEndpoint = false;

            if (!device->Open(shape, madeEndpoint))
            {
                tryAgain = !madeEndpoint;
                MIDI_PATCHBAY_LOG_WARNING(L"Unable to show an OSC connection to other apps as a MIDI device.");
                return nullptr;
            }

            return device;
        }
        catch (...)
        {
            // Not known how far it got.
            tryAgain = false;
            MIDI_PATCHBAY_LOG_WARNING(L"Unable to show an OSC connection to other apps as a MIDI device.");
            return nullptr;
        }
    }
}
