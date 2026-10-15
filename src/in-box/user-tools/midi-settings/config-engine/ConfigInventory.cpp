// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// UNSUPPORTED - DO NOT COPY THIS APPROACH. This code reads and writes the Windows MIDI Services
// configuration file directly. Microsoft does not support direct manipulation of that file by
// anything other than the in-box MIDI tools. Its format, its name, its location and the registry
// value that selects it are implementation details and can change in any release without notice,
// so this code can corrupt a configuration or stop working at any time. It exists for development
// and diagnostics inside the Windows MIDI Services repository only. Applications and third-party
// tools must use the Windows MIDI Services API (Windows.Devices.Midi2.ServiceConfig) instead.
// ============================================================================

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include "ConfigInventory.h"
#include "ConfigJson.h"
#include "ConfigKeys.h"

#include <algorithm>

namespace midisettings::configengine
{
    namespace transportids
    {
        _Use_decl_annotations_
        bool IsKnown(std::wstring_view const sectionKey) noexcept
        {
            for (auto const known : { KernelStreaming, KernelStreamingAggregate, Loopback, BasicLoopback, NetworkMidi2, RtpMidi, Bluetooth, Synthesizer })
            {
                if (SameText(sectionKey, known))
                {
                    return true;
                }
            }

            return false;
        }
    }

    namespace
    {
        void AddPicture(_Inout_ std::vector<std::wstring>& pictures, _In_ std::wstring const& name)
        {
            if (name.empty())
            {
                return;
            }

            for (auto const& existing : pictures)
            {
                if (SameText(existing, name))
                {
                    return;
                }
            }

            pictures.push_back(name);
        }

        bool IsAnyAdapter(_In_ std::wstring const& adapterId) noexcept
        {
            if (adapterId.empty())
            {
                return true;
            }

            for (auto const ch : adapterId)
            {
                if (ch != L'0' && ch != L'-' && ch != L'{' && ch != L'}')
                {
                    return false;
                }
            }

            return true;
        }

        void ReadDeviceNames(
            _Inout_ ConfigInventory& inventory,
            _In_ std::wstring const& sectionKey,
            _In_ json::JsonObject const& section)
        {
            auto const update = jsonutil::ArrayValue(section, keys::Update);

            if (update == nullptr)
            {
                return;
            }

            for (auto const& value : update)
            {
                if (value == nullptr || value.ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                auto const entry = value.GetObject();
                auto const match = jsonutil::ObjectValue(entry, keys::Match);

                if (match == nullptr)
                {
                    continue;
                }

                auto const properties = jsonutil::ObjectValue(entry, keys::CustomProperties);

                DeviceNameEntry device{};
                device.SectionKey = sectionKey;
                device.MatchIdentity = jsonutil::IdentityText(match);
                device.Name = jsonutil::StringValue(properties, keys::Name);
                device.Image = jsonutil::StringValue(properties, keys::Image);
                device.EndpointDeviceId = jsonutil::StringValue(match, keys::MatchEndpointDeviceId);
                device.DeviceInstanceId = jsonutil::StringValue(match, keys::MatchDeviceInstanceId);
                device.TransportSuppliedName = jsonutil::StringValue(match, keys::MatchTransportSuppliedName);

                AddPicture(inventory.Pictures, device.Image);

                inventory.DeviceNames.push_back(std::move(device));
            }
        }

        std::wstring StemBefore(_In_ std::wstring const& name, _In_ std::wstring_view const suffix)
        {
            if (name.size() > suffix.size() && EndsWithText(name, suffix))
            {
                return name.substr(0, name.size() - suffix.size());
            }

            return {};
        }

        void ReadLoopbacks(
            _Inout_ ConfigInventory& inventory,
            _In_ json::JsonObject const& section,
            _In_ bool const basic)
        {
            auto const create = jsonutil::ObjectValue(section, keys::Create);

            if (create == nullptr)
            {
                return;
            }

            for (auto const& pair : create)
            {
                if (pair.Value() == nullptr || pair.Value().ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                auto const definition = pair.Value().GetObject();

                LoopbackEntry loopback{};
                loopback.Basic = basic;
                loopback.AssociationId = pair.Key();

                auto const readEnd = [&loopback](json::JsonObject const& end)
                    {
                        loopback.EndpointNames.push_back(jsonutil::StringValue(end, keys::Name));
                        loopback.UniqueIds.push_back(jsonutil::StringValue(end, keys::UniqueIdentifier));

                        auto const image = jsonutil::StringValue(end, keys::Image);

                        if (!image.empty())
                        {
                            loopback.Images.push_back(image);
                        }
                    };

                if (basic)
                {
                    auto const end = jsonutil::ObjectValue(definition, keys::LoopbackEndpoint);

                    if (end == nullptr)
                    {
                        continue;
                    }

                    readEnd(end);
                    loopback.BaseName = loopback.EndpointNames.front();
                }
                else
                {
                    auto const endA = jsonutil::ObjectValue(definition, keys::LoopbackEndpointA);
                    auto const endB = jsonutil::ObjectValue(definition, keys::LoopbackEndpointB);

                    if (endA == nullptr || endB == nullptr)
                    {
                        continue;
                    }

                    readEnd(endA);
                    readEnd(endB);

                    auto const stemA = StemBefore(loopback.EndpointNames[0], L" (A)");
                    auto const stemB = StemBefore(loopback.EndpointNames[1], L" (B)");

                    if (!stemA.empty() && stemA == stemB)
                    {
                        loopback.BaseName = stemA;
                    }
                }

                for (auto const& image : loopback.Images)
                {
                    AddPicture(inventory.Pictures, image);
                }

                inventory.Loopbacks.push_back(std::move(loopback));
            }
        }

        std::wstring NetworkClientDisplayName(_In_ json::JsonObject const& match)
        {
            for (auto const key : { keys::NetworkMatchUmpEndpointName, keys::NetworkMatchServiceInstance, keys::NetworkMatchDirectHost, keys::NetworkMatchId })
            {
                auto const value = jsonutil::StringValue(match, key);

                if (!value.empty())
                {
                    return value;
                }
            }

            return {};
        }

        std::wstring RtpClientIdentity(_In_ json::JsonObject const& client)
        {
            auto const serviceInstance = jsonutil::StringValue(client, keys::ServiceInstanceName);

            if (!serviceInstance.empty())
            {
                return jsonutil::IdentityText(json::JsonValue::CreateStringValue(L"service:" + serviceInstance));
            }

            auto const address = jsonutil::StringValue(client, keys::RtpRemoteAddress);

            if (address.empty())
            {
                return {};
            }

            std::wstring port{ L"5004" };
            auto const portValue = client.TryLookup(winrt::hstring{ keys::RtpRemotePort });

            if (portValue != nullptr)
            {
                port = std::wstring{ portValue.Stringify() };
            }

            return jsonutil::IdentityText(json::JsonValue::CreateStringValue(L"address:" + address + L":" + port));
        }

        void ReadNetwork(
            _Inout_ ConfigInventory& inventory,
            _In_ json::JsonObject const& section,
            _In_ NetworkKind const kind)
        {
            auto const create = jsonutil::ObjectValue(section, keys::Create);

            if (create == nullptr)
            {
                return;
            }

            if (auto const hosts = jsonutil::ObjectValue(create, keys::Hosts))
            {
                for (auto const& pair : hosts)
                {
                    if (pair.Value() == nullptr || pair.Value().ValueType() != json::JsonValueType::Object)
                    {
                        continue;
                    }

                    auto const host = pair.Value().GetObject();

                    HostEntry entry{};
                    entry.Kind = kind;
                    entry.EntryId = pair.Key();
                    entry.Name = jsonutil::StringValue(host, keys::Name);
                    entry.ServiceInstanceName = jsonutil::StringValue(host, keys::ServiceInstanceName);
                    entry.ProductInstanceId = jsonutil::StringValue(host, keys::ProductInstanceId);
                    entry.UsesOneAdapter =
                        !IsAnyAdapter(jsonutil::StringValue(host, keys::NetworkAdapterId)) ||
                        !jsonutil::StringValue(host, keys::NetworkAdapterPhysicalAddress).empty() ||
                        !jsonutil::StringValue(host, keys::NetworkAdapterName).empty();

                    inventory.Hosts.push_back(std::move(entry));
                }
            }

            if (auto const clients = jsonutil::ObjectValue(create, keys::Clients))
            {
                for (auto const& pair : clients)
                {
                    if (pair.Value() == nullptr || pair.Value().ValueType() != json::JsonValueType::Object)
                    {
                        continue;
                    }

                    auto const client = pair.Value().GetObject();

                    ClientEntry entry{};
                    entry.Kind = kind;
                    entry.EntryId = pair.Key();

                    if (kind == NetworkKind::NetworkMidi2)
                    {
                        auto const match = jsonutil::ObjectValue(client, keys::Match);

                        entry.Identity = match == nullptr ? std::wstring{} : jsonutil::IdentityText(match);
                        entry.DisplayName = NetworkClientDisplayName(match);
                    }
                    else
                    {
                        entry.Identity = RtpClientIdentity(client);

                        for (auto const key : { keys::RtpCustomEndpointName, keys::Name, keys::ServiceInstanceName, keys::RtpRemoteAddress })
                        {
                            entry.DisplayName = jsonutil::StringValue(client, key);

                            if (!entry.DisplayName.empty())
                            {
                                break;
                            }
                        }
                    }

                    inventory.Clients.push_back(std::move(entry));
                }
            }
        }

        void ReadBluetooth(_Inout_ ConfigInventory& inventory, _In_ json::JsonObject const& section)
        {
            auto const devices = jsonutil::ArrayValue(section, keys::BluetoothDevices);

            if (devices == nullptr)
            {
                return;
            }

            for (auto const& value : devices)
            {
                if (value == nullptr || value.ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                auto const device = value.GetObject();

                BluetoothEntry entry{};
                entry.DeviceId = jsonutil::StringValue(device, keys::BluetoothDeviceId);
                entry.Name = jsonutil::StringValue(device, keys::Name);

                if (!entry.DeviceId.empty())
                {
                    inventory.BluetoothDevices.push_back(std::move(entry));
                }
            }
        }

        // Nothing but a comment left, so the section says nothing.
        bool IsEmptySection(_In_ json::JsonObject const& section)
        {
            for (auto const& pair : section)
            {
                if (!SameText(pair.Key(), keys::Comment))
                {
                    return false;
                }
            }

            return true;
        }
    }

    _Use_decl_annotations_
    size_t ConfigInventory::HostCount(NetworkKind const kind) const noexcept
    {
        return static_cast<size_t>(std::count_if(Hosts.begin(), Hosts.end(), [kind](auto const& host) { return host.Kind == kind; }));
    }

    _Use_decl_annotations_
    size_t ConfigInventory::ClientCount(NetworkKind const kind) const noexcept
    {
        return static_cast<size_t>(std::count_if(Clients.begin(), Clients.end(), [kind](auto const& client) { return client.Kind == kind; }));
    }

    bool ConfigInventory::IsEmpty() const noexcept
    {
        return DeviceNames.empty() && Loopbacks.empty() && Hosts.empty() && Clients.empty() &&
            BluetoothDevices.empty() && !HasSynthesizer && OtherSections.empty();
    }

    uint32_t ConfigInventory::PartsPresent() const noexcept
    {
        uint32_t parts{ 0 };

        if (!DeviceNames.empty())
        {
            parts |= PartBit(ConfigPart::DeviceNames);
        }

        if (!Loopbacks.empty())
        {
            parts |= PartBit(ConfigPart::Loopbacks);
        }

        if (HostCount(NetworkKind::NetworkMidi2) + ClientCount(NetworkKind::NetworkMidi2) > 0)
        {
            parts |= PartBit(ConfigPart::NetworkMidi2);
        }

        if (HostCount(NetworkKind::RtpMidi) + ClientCount(NetworkKind::RtpMidi) > 0)
        {
            parts |= PartBit(ConfigPart::RtpMidi);
        }

        if (!BluetoothDevices.empty())
        {
            parts |= PartBit(ConfigPart::Bluetooth);
        }

        if (HasSynthesizer || !OtherSections.empty())
        {
            parts |= PartBit(ConfigPart::OtherSettings);
        }

        return parts;
    }

    _Use_decl_annotations_
    std::wstring ConfigNameOf(json::JsonObject const& document) noexcept
    {
        return jsonutil::StringValue(jsonutil::ObjectValue(document, keys::Header), keys::ConfigName);
    }

    _Use_decl_annotations_
    ConfigInventory TakeInventory(json::JsonObject const& document) noexcept
    {
        ConfigInventory inventory{};

        try
        {
            auto const settings = jsonutil::ObjectValue(document, keys::TransportPluginSettings);

            if (settings == nullptr)
            {
                return inventory;
            }

            inventory.IsConfiguration = true;
            inventory.ConfigName = ConfigNameOf(document);

            for (auto const& pair : settings)
            {
                if (pair.Value() == nullptr || pair.Value().ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                std::wstring const sectionKey{ pair.Key() };
                auto const section = pair.Value().GetObject();

                ReadDeviceNames(inventory, sectionKey, section);

                if (SameText(sectionKey, transportids::Loopback))
                {
                    ReadLoopbacks(inventory, section, false);
                }
                else if (SameText(sectionKey, transportids::BasicLoopback))
                {
                    ReadLoopbacks(inventory, section, true);
                }
                else if (SameText(sectionKey, transportids::NetworkMidi2))
                {
                    ReadNetwork(inventory, section, NetworkKind::NetworkMidi2);
                }
                else if (SameText(sectionKey, transportids::RtpMidi))
                {
                    ReadNetwork(inventory, section, NetworkKind::RtpMidi);
                }
                else if (SameText(sectionKey, transportids::Bluetooth))
                {
                    ReadBluetooth(inventory, section);
                }
                else if (SameText(sectionKey, transportids::Synthesizer))
                {
                    inventory.HasSynthesizer = true;
                    inventory.SynthMode = jsonutil::StringValue(section, keys::SynthMode);
                }
                else if (!transportids::IsKnown(sectionKey))
                {
                    inventory.OtherSections.push_back(sectionKey);
                }
            }
        }
        catch (...)
        {
            inventory = {};
        }

        return inventory;
    }

    _Use_decl_annotations_
    ConfigInventory TakeInventory(std::string_view const bytes) noexcept
    {
        auto const document = jsonutil::ParseObject(bytes);

        if (document == nullptr)
        {
            return {};
        }

        return TakeInventory(document);
    }

    _Use_decl_annotations_
    json::JsonObject WithOnlyParts(json::JsonObject const& document, uint32_t const parts) noexcept
    {
        try
        {
            auto copy = jsonutil::CopyObject(document);
            auto const settings = jsonutil::ObjectValue(copy, keys::TransportPluginSettings);

            if (settings == nullptr)
            {
                return copy;
            }

            auto const wants = [parts](ConfigPart const part) { return (parts & PartBit(part)) != 0; };

            std::vector<std::wstring> emptied{};

            for (auto const& pair : settings)
            {
                if (pair.Value() == nullptr || pair.Value().ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                std::wstring const sectionKey{ pair.Key() };
                auto const section = pair.Value().GetObject();

                if (!transportids::IsKnown(sectionKey))
                {
                    if (!wants(ConfigPart::OtherSettings))
                    {
                        emptied.push_back(sectionKey);
                    }

                    continue;
                }

                if (!wants(ConfigPart::DeviceNames) && section.HasKey(winrt::hstring{ keys::Update }))
                {
                    section.Remove(winrt::hstring{ keys::Update });
                }

                auto const removeKey = [&section](std::wstring_view const key)
                    {
                        if (section.HasKey(winrt::hstring{ key }))
                        {
                            section.Remove(winrt::hstring{ key });
                        }
                    };

                if (SameText(sectionKey, transportids::Loopback) || SameText(sectionKey, transportids::BasicLoopback))
                {
                    if (!wants(ConfigPart::Loopbacks))
                    {
                        removeKey(keys::Create);
                    }
                }
                else if (SameText(sectionKey, transportids::NetworkMidi2))
                {
                    if (!wants(ConfigPart::NetworkMidi2))
                    {
                        removeKey(keys::Create);
                        removeKey(keys::UpdateEntries);
                    }
                }
                else if (SameText(sectionKey, transportids::RtpMidi))
                {
                    if (!wants(ConfigPart::RtpMidi))
                    {
                        removeKey(keys::Create);
                        removeKey(keys::UpdateEntries);
                    }
                }
                else if (SameText(sectionKey, transportids::Bluetooth))
                {
                    if (!wants(ConfigPart::Bluetooth))
                    {
                        removeKey(keys::BluetoothDevices);
                    }
                }
                else if (SameText(sectionKey, transportids::Synthesizer))
                {
                    if (!wants(ConfigPart::OtherSettings))
                    {
                        removeKey(keys::SynthMode);
                    }
                }

                if (IsEmptySection(section))
                {
                    emptied.push_back(sectionKey);
                }
            }

            for (auto const& key : emptied)
            {
                settings.Remove(winrt::hstring{ key });
            }

            return copy;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    std::vector<std::wstring> PicturesUsedBy(ConfigInventory const& inventory, uint32_t const parts)
    {
        std::vector<std::wstring> pictures{};

        if ((parts & PartBit(ConfigPart::DeviceNames)) != 0)
        {
            for (auto const& device : inventory.DeviceNames)
            {
                AddPicture(pictures, device.Image);
            }
        }

        if ((parts & PartBit(ConfigPart::Loopbacks)) != 0)
        {
            for (auto const& loopback : inventory.Loopbacks)
            {
                for (auto const& image : loopback.Images)
                {
                    AddPicture(pictures, image);
                }
            }
        }

        return pictures;
    }

    _Use_decl_annotations_
    json::JsonObject WithConfigName(json::JsonObject const& document, std::wstring const& configName) noexcept
    {
        try
        {
            auto copy = jsonutil::CopyObject(document);

            if (copy == nullptr)
            {
                return nullptr;
            }

            auto const header = jsonutil::EnsureObject(copy, keys::Header);

            if (header == nullptr)
            {
                return nullptr;
            }

            header.SetNamedValue(winrt::hstring{ keys::ConfigName }, json::JsonValue::CreateStringValue(configName));

            return copy;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    std::string EmptyConfiguration(std::wstring const& configName) noexcept
    {
        try
        {
            json::JsonObject document{};
            json::JsonObject header{};

            // the same header the app has always written for a new file
            header.SetNamedValue(winrt::hstring{ keys::Comment },
                json::JsonValue::CreateStringValue(L"NOTE: All json keys are case-sensitive, including GUIDs."));
            header.SetNamedValue(winrt::hstring{ keys::ConfigName }, json::JsonValue::CreateStringValue(configName));
            header.SetNamedValue(L"product", json::JsonValue::CreateStringValue(L"Windows MIDI Services"));
            header.SetNamedValue(L"fileVersion", json::JsonValue::CreateNumberValue(1.0));

            document.SetNamedValue(winrt::hstring{ keys::Header }, header);
            document.SetNamedValue(winrt::hstring{ keys::TransportPluginSettings }, json::JsonObject{});

            return jsonutil::Serialize(document);
        }
        catch (...)
        {
            return {};
        }
    }
}
