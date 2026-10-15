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

#include "ImportApply.h"
#include "ConfigFiles.h"
#include "ConfigInventory.h"
#include "ConfigJson.h"
#include "ConfigKeys.h"
#include "ConfigWrite.h"

#include <algorithm>

namespace midisettings::configengine
{
    namespace
    {
        // the prefix and the length the API gives a host's product instance ID
        constexpr std::wstring_view ProductInstanceIdPrefix{ L"winmidisrv" };
        constexpr size_t ProductInstanceIdLength{ 42 };

        winrt::hstring Key(_In_ std::wstring_view const key)
        {
            return winrt::hstring{ key };
        }

        json::JsonArray EnsureArray(_In_ json::JsonObject const& object, _In_ std::wstring_view const key)
        {
            auto existing = jsonutil::ArrayValue(object, key);

            if (existing != nullptr)
            {
                return existing;
            }

            json::JsonArray added{};
            object.SetNamedValue(Key(key), added);

            return added;
        }

        bool HasLowercaseLetter(_In_ std::wstring_view const text) noexcept
        {
            return std::any_of(text.begin(), text.end(), [](wchar_t const ch) { return ch >= L'a' && ch <= L'z'; });
        }

        // An entry ID that isn't used here, in the same style as the one it replaces.
        std::wstring FreeEntryKey(
            _In_ std::wstring const& key,
            _In_ std::initializer_list<json::JsonObject> const lists)
        {
            auto const taken = [&lists](std::wstring const& candidate)
                {
                    return std::any_of(lists.begin(), lists.end(), [&candidate](auto const& list) { return !jsonutil::FindKey(list, candidate).empty(); });
                };

            if (!taken(key))
            {
                return key;
            }

            return jsonutil::NewGuidText(!HasLowercaseLetter(key));
        }

        void RenameImage(_In_ json::JsonObject const& owner, _In_ PictureRenames const& renames)
        {
            auto const image = jsonutil::StringValue(owner, keys::Image);

            if (image.empty())
            {
                return;
            }

            for (auto const& rename : renames)
            {
                if (SameText(rename.first, image))
                {
                    owner.SetNamedValue(Key(keys::Image), json::JsonValue::CreateStringValue(rename.second));
                    return;
                }
            }
        }

        json::JsonObject FindDeviceEntry(_In_ json::JsonObject const& section, _In_ std::wstring const& matchIdentity)
        {
            auto const update = jsonutil::ArrayValue(section, keys::Update);

            if (update == nullptr)
            {
                return nullptr;
            }

            for (auto const& value : update)
            {
                if (value == nullptr || value.ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                auto const entry = value.GetObject();
                auto const match = jsonutil::ObjectValue(entry, keys::Match);

                if (match != nullptr && jsonutil::IdentityText(match) == matchIdentity)
                {
                    return entry;
                }
            }

            return nullptr;
        }

        void RemoveDeviceEntries(_In_ json::JsonArray const& update, _In_ std::wstring const& matchIdentity)
        {
            for (uint32_t index = update.Size(); index > 0; index--)
            {
                auto const value = update.GetAt(index - 1);

                if (value == nullptr || value.ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                auto const match = jsonutil::ObjectValue(value.GetObject(), keys::Match);

                if (match != nullptr && jsonutil::IdentityText(match) == matchIdentity)
                {
                    update.RemoveAt(index - 1);
                }
            }
        }

        std::vector<std::wstring> UniqueIdsIn(_In_ json::JsonObject const& create)
        {
            std::vector<std::wstring> ids{};

            if (create == nullptr)
            {
                return ids;
            }

            for (auto const& pair : create)
            {
                if (pair.Value() == nullptr || pair.Value().ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                for (auto const end : { keys::LoopbackEndpointA, keys::LoopbackEndpointB, keys::LoopbackEndpoint })
                {
                    auto const id = jsonutil::StringValue(jsonutil::ObjectValue(pair.Value().GetObject(), end), keys::UniqueIdentifier);

                    if (!id.empty())
                    {
                        ids.push_back(id);
                    }
                }
            }

            return ids;
        }

        std::wstring WithoutDashes(_In_ std::wstring const& text)
        {
            std::wstring result{};

            for (auto const ch : text)
            {
                if (ch != L'-')
                {
                    result += ch;
                }
            }

            return result;
        }

        std::wstring ProductInstanceIdFor(_In_ std::wstring const& entryKey)
        {
            auto id = jsonutil::GuidTextWithoutBraces(entryKey);

            ::LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, id.c_str(), static_cast<int>(id.size()), id.data(), static_cast<int>(id.size()), nullptr, nullptr, 0);

            auto const room = ProductInstanceIdLength - ProductInstanceIdPrefix.size();

            if (id.size() > room)
            {
                id.resize(room);
            }

            return std::wstring{ ProductInstanceIdPrefix } + id;
        }

        bool StartsWithText(_In_ std::wstring_view const text, _In_ std::wstring_view const prefix) noexcept
        {
            return text.size() >= prefix.size() && SameText(text.substr(0, prefix.size()), prefix);
        }

        // Carries an entry's changes made after it was created over to the ID it's added under.
        void CarryUpdateEntries(
            _In_ json::JsonObject const& packageSection,
            _In_ json::JsonObject const& targetSection,
            _In_ std::wstring_view const listKey,
            _In_ std::wstring const& oldKey,
            _In_ std::wstring const& newKey)
        {
            auto const packageList = jsonutil::ObjectValue(jsonutil::ObjectValue(packageSection, keys::UpdateEntries), listKey);
            auto const changes = jsonutil::ObjectValue(packageList, jsonutil::FindKey(packageList, oldKey));

            if (changes == nullptr)
            {
                return;
            }

            auto const updateEntries = jsonutil::EnsureObject(targetSection, keys::UpdateEntries);
            auto const targetList = jsonutil::EnsureObject(updateEntries, listKey);

            if (targetList != nullptr)
            {
                targetList.SetNamedValue(Key(newKey), jsonutil::CopyObject(changes));
            }
        }

        ResultLine Line(_In_ ResultCode const code, _In_ ResultTone const tone)
        {
            ResultLine line{};
            line.Code = code;
            line.Tone = tone;

            return line;
        }

        std::wstring LoopbackDisplayName(_In_ PlanItem const& item)
        {
            return item.BaseName.empty() ? item.Name : item.BaseName;
        }

        class Merger
        {
        public:
            Merger(
                _In_ ImportPlan const& plan,
                _In_ ImportChoices const& choices,
                _In_ PictureRenames const& renames,
                _Inout_ std::vector<ResultLine>& lines,
                _In_ json::JsonObject const& result,
                _In_ json::JsonObject const& package) :
                m_plan(plan),
                m_choices(choices),
                m_renames(renames),
                m_lines(lines),
                m_settings(jsonutil::EnsureObject(result, keys::TransportPluginSettings)),
                m_packageSettings(jsonutil::ObjectValue(package, keys::TransportPluginSettings))
            {
            }

            void Merge()
            {
                MergeDeviceNames();
                MergeLoopbacks();
                MergeNetwork(NetworkKind::NetworkMidi2);
                MergeNetwork(NetworkKind::RtpMidi);
                MergeBluetooth();
                MergeSynthesizer();
                MergeOtherSections();
            }

        private:
            json::JsonObject PackageSection(_In_ std::wstring_view const key) const
            {
                return jsonutil::ObjectValue(m_packageSettings, jsonutil::FindKey(m_packageSettings, key));
            }

            // The section here, or a new one carrying the package's comment.
            json::JsonObject TargetSection(_In_ std::wstring_view const key) const
            {
                auto const existingKey = jsonutil::FindKey(m_settings, key);

                if (!existingKey.empty())
                {
                    auto existing = jsonutil::ObjectValue(m_settings, existingKey);

                    if (existing != nullptr)
                    {
                        return existing;
                    }
                }

                auto const packageKey = jsonutil::FindKey(m_packageSettings, key);
                auto const comment = jsonutil::StringValue(jsonutil::ObjectValue(m_packageSettings, packageKey), keys::Comment);

                json::JsonObject added{};

                if (!comment.empty())
                {
                    added.SetNamedValue(Key(keys::Comment), json::JsonValue::CreateStringValue(comment));
                }

                m_settings.SetNamedValue(Key(packageKey.empty() ? std::wstring{ key } : packageKey), added);

                return added;
            }

            bool IntoNew() const noexcept { return m_choices.Target == ImportTarget::NewConfiguration; }

            void MergeDeviceNames()
            {
                uint32_t added{ 0 };
                uint32_t kept{ 0 };
                uint32_t replaced{ 0 };
                uint32_t removed{ 0 };
                bool any{ false };

                if (!IntoNew() && m_choices.DeviceNames == DeviceNamesChoice::ReplaceMine)
                {
                    for (auto const& pair : m_settings)
                    {
                        if (pair.Value() != nullptr && pair.Value().ValueType() == json::JsonValueType::Object)
                        {
                            auto const section = pair.Value().GetObject();

                            if (section.HasKey(Key(keys::Update)))
                            {
                                section.Remove(Key(keys::Update));
                            }
                        }
                    }
                }

                for (auto const& item : m_plan.Items)
                {
                    if (item.Kind != ItemKind::DeviceName)
                    {
                        continue;
                    }

                    auto const outcome = OutcomeFor(item, m_choices);

                    if (item.Status == ItemStatus::OnlyHere)
                    {
                        removed += outcome == ItemOutcome::LosesYourName ? 1 : 0;
                        continue;
                    }

                    any = true;

                    if (outcome == ItemOutcome::KeepsYourName)
                    {
                        kept++;
                    }

                    if (!IsImported(outcome))
                    {
                        continue;
                    }

                    auto const entry = jsonutil::CopyObject(FindDeviceEntry(PackageSection(item.SectionKey), item.MatchIdentity));

                    if (entry == nullptr)
                    {
                        continue;
                    }

                    RenameImage(jsonutil::ObjectValue(entry, keys::CustomProperties), m_renames);

                    auto const update = EnsureArray(TargetSection(item.SectionKey), keys::Update);

                    if (outcome == ItemOutcome::GetsThePackagesName)
                    {
                        RemoveDeviceEntries(update, item.MatchIdentity);
                        replaced++;
                    }
                    else
                    {
                        added++;
                    }

                    update.Append(entry);
                }

                if (!any)
                {
                    return;
                }

                if (m_choices.DeviceNames == DeviceNamesChoice::DontImport)
                {
                    m_lines.push_back(Line(ResultCode::DeviceNamesNotImported, ResultTone::Neutral));
                    return;
                }

                auto line = Line(ResultCode::DeviceNames, ResultTone::Done);
                line.Count = added;
                line.Count3 = kept;
                line.Count4 = replaced;
                line.Count5 = removed;

                m_lines.push_back(std::move(line));
            }

            void MergeLoopbacks()
            {
                bool any{ false };

                for (auto const& item : m_plan.Items)
                {
                    if (item.Kind != ItemKind::Loopback)
                    {
                        continue;
                    }

                    any = true;

                    auto const outcome = OutcomeFor(item, m_choices);

                    if (outcome == ItemOutcome::LeftOutNameTaken)
                    {
                        auto line = Line(ResultCode::LoopbackLeftOut, ResultTone::Warning);
                        line.Name = LoopbackDisplayName(item);
                        m_lines.push_back(std::move(line));
                        continue;
                    }

                    if (!IsImported(outcome))
                    {
                        continue;
                    }

                    auto const packageCreate = jsonutil::ObjectValue(PackageSection(item.SectionKey), keys::Create);
                    auto const definition = jsonutil::CopyObject(jsonutil::ObjectValue(packageCreate, jsonutil::FindKey(packageCreate, item.EntryKey)));

                    if (definition == nullptr)
                    {
                        continue;
                    }

                    auto const target = TargetSection(item.SectionKey);
                    auto const create = jsonutil::EnsureObject(target, keys::Create);

                    // The association ID ties the two ends together, so a new loopback can't
                    // share one with a loopback that's already here.
                    auto const newKey = FreeEntryKey(item.EntryKey, { create });

                    std::vector<json::JsonObject> ends{};

                    for (auto const end : { keys::LoopbackEndpointA, keys::LoopbackEndpointB, keys::LoopbackEndpoint })
                    {
                        if (auto const endObject = jsonutil::ObjectValue(definition, end))
                        {
                            ends.push_back(endObject);
                        }
                    }

                    if (outcome == ItemOutcome::AddedUnderNewName)
                    {
                        for (size_t index = 0; index < ends.size() && index < item.FreeEndpointNames.size(); index++)
                        {
                            ends[index].SetNamedValue(Key(keys::Name), json::JsonValue::CreateStringValue(item.FreeEndpointNames[index]));
                        }
                    }

                    // The unique ID is what the endpoint's ID is built from, so two loopbacks
                    // can't share one either.
                    auto const idsHere = UniqueIdsIn(create);
                    std::wstring newUniqueId{};

                    for (auto const& end : ends)
                    {
                        auto const id = jsonutil::StringValue(end, keys::UniqueIdentifier);

                        auto const taken = !id.empty() && std::any_of(idsHere.begin(), idsHere.end(), [&id](auto const& existing) { return SameText(existing, id); });

                        if (taken)
                        {
                            if (newUniqueId.empty())
                            {
                                auto const source = newKey != item.EntryKey ? newKey : jsonutil::NewGuidText(true);
                                newUniqueId = WithoutDashes(jsonutil::GuidTextWithoutBraces(source));
                            }

                            end.SetNamedValue(Key(keys::UniqueIdentifier), json::JsonValue::CreateStringValue(newUniqueId));
                        }

                        RenameImage(end, m_renames);
                    }

                    create.SetNamedValue(Key(newKey), definition);

                    auto line = Line(outcome == ItemOutcome::AddedUnderNewName ? ResultCode::LoopbackAddedAs : ResultCode::LoopbackAdded, ResultTone::Done);
                    line.Name = LoopbackDisplayName(item);
                    line.NewName = item.FreeName;
                    m_lines.push_back(std::move(line));
                }

                if (any && m_choices.Loopbacks == PartChoice::DontImport)
                {
                    m_lines.push_back(Line(ResultCode::LoopbacksNotImported, ResultTone::Neutral));
                }
            }

            void MergeNetwork(_In_ NetworkKind const kind)
            {
                auto const rtp = kind == NetworkKind::RtpMidi;
                auto const part = rtp ? m_choices.RtpMidi : m_choices.NetworkMidi2;
                auto const sectionKey = rtp ? transportids::RtpMidi : transportids::NetworkMidi2;
                auto const savedForLater = rtp && !m_plan.RtpInstalled;

                bool any{ false };
                uint32_t saved{ 0 };

                for (auto const& item : m_plan.Items)
                {
                    if ((item.Kind != ItemKind::Host && item.Kind != ItemKind::Client) || item.Network != kind)
                    {
                        continue;
                    }

                    any = true;

                    auto const outcome = OutcomeFor(item, m_choices);

                    if (outcome == ItemOutcome::LeftOutNameTaken)
                    {
                        auto line = Line(ResultCode::HostLeftOut, ResultTone::Warning);
                        line.Network = kind;
                        line.Name = item.Name.empty() ? m_plan.ThisComputerName : item.Name;
                        m_lines.push_back(std::move(line));
                        continue;
                    }

                    if (!IsImported(outcome))
                    {
                        continue;
                    }

                    auto const packageSection = PackageSection(sectionKey);
                    auto const listKey = item.Kind == ItemKind::Host ? keys::Hosts : keys::Clients;
                    auto const packageList = jsonutil::ObjectValue(jsonutil::ObjectValue(packageSection, keys::Create), listKey);
                    auto const definition = jsonutil::CopyObject(jsonutil::ObjectValue(packageList, jsonutil::FindKey(packageList, item.EntryKey)));

                    if (definition == nullptr)
                    {
                        continue;
                    }

                    auto const target = TargetSection(sectionKey);
                    auto const create = jsonutil::EnsureObject(target, keys::Create);
                    auto const hosts = jsonutil::EnsureObject(create, keys::Hosts);
                    auto const clients = jsonutil::EnsureObject(create, keys::Clients);

                    // A host that takes this PC's name takes a new identity too. The product
                    // instance ID is built from the entry ID, so the entry ID has to change with it.
                    auto const newKey = item.Kind == ItemKind::Host && outcome == ItemOutcome::TakesThisPcName ?
                        jsonutil::NewGuidText(!HasLowercaseLetter(item.EntryKey)) :
                        FreeEntryKey(item.EntryKey, { hosts, clients });

                    std::wstring newName{};

                    if (item.Kind == ItemKind::Host)
                    {
                        if (outcome == ItemOutcome::TakesThisPcName || outcome == ItemOutcome::AddedUnderNewName)
                        {
                            if (!item.FreeName.empty())
                            {
                                definition.SetNamedValue(Key(keys::Name), json::JsonValue::CreateStringValue(item.FreeName));
                                newName = item.FreeName;
                            }

                            if (!item.FreeServiceInstanceName.empty())
                            {
                                definition.SetNamedValue(Key(keys::ServiceInstanceName), json::JsonValue::CreateStringValue(item.FreeServiceInstanceName));
                            }
                        }

                        if (newName.empty() && outcome == ItemOutcome::TakesThisPcName)
                        {
                            newName = m_plan.ThisComputerName;
                        }

                        // The product instance ID identifies the host on the network, so it's
                        // built again the way the API builds one for a new host when the host
                        // takes this PC's identity, when it was built from an entry ID that had
                        // to change, or when a host here already uses it.
                        auto const productId = jsonutil::StringValue(definition, keys::ProductInstanceId);

                        if (!rtp && !productId.empty() &&
                            (outcome == ItemOutcome::TakesThisPcName ||
                                (newKey != item.EntryKey && StartsWithText(productId, ProductInstanceIdPrefix)) ||
                                ProductIdUsedHere(productId)))
                        {
                            definition.SetNamedValue(Key(keys::ProductInstanceId), json::JsonValue::CreateStringValue(ProductInstanceIdFor(newKey)));
                        }

                        // the adapter it was set to is on the other PC
                        if (item.UsesOtherAdapter)
                        {
                            definition.SetNamedValue(Key(keys::AllowNetworkAdapterFallback), json::JsonValue::CreateBooleanValue(true));
                        }

                        hosts.SetNamedValue(Key(newKey), definition);
                    }
                    else
                    {
                        clients.SetNamedValue(Key(newKey), definition);
                    }

                    CarryUpdateEntries(packageSection, target, listKey, item.EntryKey, newKey);

                    if (savedForLater)
                    {
                        saved++;
                        continue;
                    }

                    ResultCode code{ ResultCode::HostAdded };

                    switch (outcome)
                    {
                    case ItemOutcome::TakesThisPcName:  code = ResultCode::HostAddedAsThisPc; break;
                    case ItemOutcome::AddedUnderNewName: code = ResultCode::HostAddedAs; break;
                    case ItemOutcome::KeepsOtherPcName: code = ResultCode::HostKeptOtherPcName; break;
                    default: code = item.Kind == ItemKind::Host ? ResultCode::HostAdded : ResultCode::ClientAdded; break;
                    }

                    auto line = Line(code, ResultTone::Done);
                    line.Network = kind;
                    line.Name = item.Name.empty() ? m_plan.ThisComputerName : item.Name;
                    line.NewName = newName;
                    m_lines.push_back(std::move(line));
                }

                if (!any)
                {
                    return;
                }

                if (part == PartChoice::DontImport)
                {
                    auto line = Line(ResultCode::NetworkNotImported, ResultTone::Neutral);
                    line.Network = kind;
                    m_lines.push_back(std::move(line));
                    return;
                }

                if (saved > 0)
                {
                    auto line = Line(ResultCode::RtpSavedForLater, ResultTone::Waiting);
                    line.Network = kind;
                    line.Count = saved;
                    m_lines.push_back(std::move(line));
                }
            }

            bool ProductIdUsedHere(_In_ std::wstring const& productId) const
            {
                return std::any_of(m_plan.Here.Hosts.begin(), m_plan.Here.Hosts.end(),
                    [&productId](auto const& host) { return !host.ProductInstanceId.empty() && SameText(host.ProductInstanceId, productId); });
            }

            void MergeBluetooth()
            {
                bool any{ false };
                uint32_t added{ 0 };

                auto const packageDevices = jsonutil::ArrayValue(PackageSection(transportids::Bluetooth), keys::BluetoothDevices);

                for (auto const& item : m_plan.Items)
                {
                    if (item.Kind != ItemKind::BluetoothDevice)
                    {
                        continue;
                    }

                    any = true;

                    if (!IsImported(OutcomeFor(item, m_choices)) || packageDevices == nullptr)
                    {
                        continue;
                    }

                    for (auto const& value : packageDevices)
                    {
                        if (value == nullptr || value.ValueType() != json::JsonValueType::Object)
                        {
                            continue;
                        }

                        auto const device = value.GetObject();

                        if (SameText(jsonutil::StringValue(device, keys::BluetoothDeviceId), item.EntryKey))
                        {
                            EnsureArray(TargetSection(transportids::Bluetooth), keys::BluetoothDevices).Append(jsonutil::CopyObject(device));
                            added++;
                            break;
                        }
                    }
                }

                if (!any)
                {
                    return;
                }

                if (m_choices.Bluetooth == PartChoice::DontImport)
                {
                    m_lines.push_back(Line(ResultCode::BluetoothNotImported, ResultTone::Neutral));
                }
                else if (added > 0)
                {
                    auto line = Line(ResultCode::BluetoothAdded, ResultTone::Done);
                    line.Count = added;
                    m_lines.push_back(std::move(line));
                }
            }

            void MergeSynthesizer()
            {
                // Only the mode. The audio output settings stay with the PC, because they depend on
                // its sound hardware.
                if (!SynthModeWillChange(m_plan, m_choices))
                {
                    return;
                }

                TargetSection(transportids::Synthesizer).SetNamedValue(Key(keys::SynthMode), json::JsonValue::CreateStringValue(m_plan.PackageSynthMode));

                auto line = Line(ResultCode::SynthModeSet, ResultTone::Done);
                line.Name = m_plan.PackageSynthMode;
                m_lines.push_back(std::move(line));
            }

            // A section for a transport this engine doesn't know comes across whole, and only when
            // this PC has no section for it.
            void MergeOtherSections()
            {
                for (auto const& item : m_plan.Items)
                {
                    if (item.Kind != ItemKind::OtherSection || !IsImported(OutcomeFor(item, m_choices)))
                    {
                        continue;
                    }

                    if (!jsonutil::FindKey(m_settings, item.SectionKey).empty())
                    {
                        continue;
                    }

                    auto const section = jsonutil::CopyObject(PackageSection(item.SectionKey));

                    if (section == nullptr)
                    {
                        continue;
                    }

                    m_settings.SetNamedValue(Key(item.SectionKey), section);

                    auto line = Line(ResultCode::OtherSectionAdded, ResultTone::Done);
                    line.Name = item.SectionKey;
                    m_lines.push_back(std::move(line));
                }
            }

            ImportPlan const& m_plan;
            ImportChoices const& m_choices;
            PictureRenames const& m_renames;
            std::vector<ResultLine>& m_lines;
            json::JsonObject m_settings{ nullptr };
            json::JsonObject m_packageSettings{ nullptr };
        };
    }

    _Use_decl_annotations_
    std::vector<std::wstring> PicturesToImport(ImportPlan const& plan, ImportChoices const& choices)
    {
        std::vector<std::wstring> pictures{};

        auto const add = [&pictures](std::wstring const& name)
            {
                if (!name.empty() && std::none_of(pictures.begin(), pictures.end(), [&name](auto const& existing) { return SameText(existing, name); }))
                {
                    pictures.push_back(name);
                }
            };

        for (auto const& item : plan.Items)
        {
            if (!IsImported(OutcomeFor(item, choices)))
            {
                continue;
            }

            if (item.Kind == ItemKind::DeviceName)
            {
                add(item.Image);
            }
            else if (item.Kind == ItemKind::Loopback)
            {
                for (auto const& loopback : plan.Package.Loopbacks)
                {
                    if (SameText(loopback.AssociationId, item.EntryKey) &&
                        SameText(loopback.Basic ? transportids::BasicLoopback : transportids::Loopback, item.SectionKey))
                    {
                        for (auto const& image : loopback.Images)
                        {
                            add(image);
                        }
                    }
                }
            }
        }

        return pictures;
    }

    _Use_decl_annotations_
    json::JsonObject MergeImport(
        ImportPlan const& plan,
        PackageContents const& contents,
        ImportChoices const& choices,
        PictureRenames const& renames,
        std::vector<ResultLine>& lines) noexcept
    {
        try
        {
            auto const intoNew = choices.Target == ImportTarget::NewConfiguration;

            auto const result = intoNew ?
                jsonutil::ParseObject(EmptyConfiguration(plan.NewConfigurationName)) :
                jsonutil::ParseObject(plan.PlannedBytes);

            auto const package = jsonutil::ParseObject(contents.ConfigurationBytes);

            if (result == nullptr || package == nullptr)
            {
                return nullptr;
            }

            Merger merger{ plan, choices, renames, lines, result, package };
            merger.Merge();

            return result;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    ImportResult ApplyImport(
        ConfigEnvironment const& environment,
        PackageContents const& contents,
        ImportPlan const& plan,
        ImportChoices const& choices) noexcept
    {
        ImportResult result{};

        try
        {
            result.Target = choices.Target;

            auto const intoNew = choices.Target == ImportTarget::NewConfiguration;

            if (!intoNew && !plan.HasTarget)
            {
                result.Error = ConfigError::NotFound;
                return result;
            }

            // A full backup of exactly what the plan saw, before anything changes.
            if (!intoNew)
            {
                auto backup = MakeBackupOfBytes(
                    environment,
                    plan.TargetFileName,
                    plan.PlannedBytes,
                    BackupReason::BeforeImport,
                    plan.Source.ComputerName,
                    plan.Source.ConfigName.empty() ? plan.Package.ConfigName : plan.Source.ConfigName);

                if (!backup.Succeeded())
                {
                    result.Error = ConfigError::BackupFailed;
                    result.SystemError = backup.SystemError;
                    return result;
                }

                result.Backup = std::move(backup.Backup);
            }

            auto const stored = StorePictures(environment, contents, PicturesToImport(plan, choices));

            std::vector<ResultLine> lines{};

            auto const merged = MergeImport(plan, contents, choices, stored.Renames, lines);

            if (merged == nullptr)
            {
                result.Error = ConfigError::Unexpected;
                return result;
            }

            for (auto& line : lines)
            {
                if (line.Code == ResultCode::DeviceNames)
                {
                    line.Count2 = stored.Stored;
                }
            }

            auto const bytes = jsonutil::Serialize(merged);

            // still a configuration the service can read
            if (!LooksLikeConfiguration(bytes))
            {
                result.Error = ConfigError::Unexpected;
                return result;
            }

            if (intoNew)
            {
                auto const created = CreateConfigurationFromBytes(environment, plan.NewConfigurationName, bytes);

                if (!created.Succeeded())
                {
                    result.Error = created.Error;
                    result.SystemError = created.SystemError;
                    return result;
                }

                result.ConfigFileName = created.FileName;
                result.ConfigName = plan.NewConfigurationName;
            }
            else
            {
                auto const written = WriteInPlace(environment.ConfigFolder + plan.TargetFileName, bytes, &plan.PlannedBytes);

                if (!written.Succeeded())
                {
                    result.Error = written.Error;
                    result.SystemError = written.SystemError;
                    return result;
                }

                result.ConfigFileName = plan.TargetFileName;
                result.ConfigName = plan.TargetConfigName;
            }

            if (choices.OtherSettings == OtherSettingsChoice::UseThePackages && plan.PackagePortNaming != PortNaming::None)
            {
                if (!plan.PackagePortNamingUsable)
                {
                    lines.push_back(Line(ResultCode::PortNamingNotUsable, ResultTone::Warning));
                }
                else if (PortNamingWillChange(plan, choices))
                {
                    if (environment.WritePortNaming && environment.WritePortNaming(plan.PackagePortNaming))
                    {
                        result.PortNamingChanged = true;

                        auto line = Line(ResultCode::PortNamingSet, ResultTone::Warning);
                        line.Name = PortNamingName(plan.PackagePortNaming);
                        lines.push_back(std::move(line));
                    }
                    else
                    {
                        lines.push_back(Line(ResultCode::PortNamingFailed, ResultTone::Warning));
                    }
                }
            }
            else if (choices.OtherSettings == OtherSettingsChoice::KeepMine &&
                (plan.PackageHasSynthMode || plan.PackagePortNaming != PortNaming::None))
            {
                lines.push_back(Line(ResultCode::OtherSettingsKept, ResultTone::Done));
            }

            if (stored.Failed > 0)
            {
                auto line = Line(ResultCode::PicturesFailed, ResultTone::Warning);
                line.Count = stored.Failed;
                lines.push_back(std::move(line));
            }

            result.Lines = std::move(lines);

            return result;
        }
        catch (...)
        {
            result.Error = ConfigError::Unexpected;
            return result;
        }
    }
}
