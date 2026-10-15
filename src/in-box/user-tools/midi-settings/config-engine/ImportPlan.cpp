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

#include "ImportPlan.h"
#include "ConfigFiles.h"
#include "ConfigJson.h"
#include "ConfigWrite.h"

#include <algorithm>

namespace midisettings::configengine
{
    namespace
    {
        constexpr uint32_t MaximumFreeNameAttempts{ 99 };

        bool Contains(_In_ std::vector<std::wstring> const& list, _In_ std::wstring_view const value) noexcept
        {
            return std::any_of(list.begin(), list.end(), [value](auto const& existing) { return SameText(existing, value); });
        }

        std::wstring ItemIdFor(_In_ std::wstring_view const prefix, _In_ std::wstring_view const key)
        {
            return std::wstring{ prefix } + std::wstring{ key };
        }

        wchar_t NetworkLetter(_In_ NetworkKind const kind) noexcept
        {
            return kind == NetworkKind::RtpMidi ? L'r' : L'n';
        }

        void PlanDeviceNames(_Inout_ ImportPlan& plan, _In_ PackageContents const& contents)
        {
            std::vector<bool> hereMatched(plan.Here.DeviceNames.size(), false);
            std::vector<std::wstring> pictures{};

            for (size_t index = 0; index < plan.Package.DeviceNames.size(); index++)
            {
                auto const& device = plan.Package.DeviceNames[index];

                PlanItem item{};
                item.Id = ItemIdFor(L"device:", std::to_wstring(index));
                item.Part = ConfigPart::DeviceNames;
                item.Kind = ItemKind::DeviceName;
                item.Status = ItemStatus::New;
                item.Name = device.Name;
                item.Image = device.Image;
                item.EndpointDeviceId = device.EndpointDeviceId;
                item.DeviceInstanceId = device.DeviceInstanceId;
                item.TransportSuppliedName = device.TransportSuppliedName;
                item.SectionKey = device.SectionKey;
                item.MatchIdentity = device.MatchIdentity;

                for (size_t hereIndex = 0; hereIndex < plan.Here.DeviceNames.size(); hereIndex++)
                {
                    auto const& here = plan.Here.DeviceNames[hereIndex];

                    if (!hereMatched[hereIndex] &&
                        SameText(here.SectionKey, device.SectionKey) &&
                        here.MatchIdentity == device.MatchIdentity)
                    {
                        hereMatched[hereIndex] = true;

                        item.Status = ItemStatus::InBoth;
                        item.YourName = here.Name;
                        item.YourImage = here.Image;
                        break;
                    }
                }

                if (!device.Image.empty() && contents.FindPicture(device.Image) != nullptr && !Contains(pictures, device.Image))
                {
                    pictures.push_back(device.Image);
                }

                plan.Items.push_back(std::move(item));
            }

            plan.PackageDevicePictureCount = static_cast<uint32_t>(pictures.size());

            // What this PC has that the package doesn't name, which only matters when the
            // customer chooses to replace their names with the package's.
            for (size_t hereIndex = 0; hereIndex < plan.Here.DeviceNames.size(); hereIndex++)
            {
                if (hereMatched[hereIndex])
                {
                    continue;
                }

                auto const& here = plan.Here.DeviceNames[hereIndex];

                PlanItem item{};
                item.Id = ItemIdFor(L"device-here:", std::to_wstring(hereIndex));
                item.Part = ConfigPart::DeviceNames;
                item.Kind = ItemKind::DeviceName;
                item.Status = ItemStatus::OnlyHere;
                item.YourName = here.Name;
                item.YourImage = here.Image;
                item.EndpointDeviceId = here.EndpointDeviceId;
                item.DeviceInstanceId = here.DeviceInstanceId;
                item.TransportSuppliedName = here.TransportSuppliedName;
                item.SectionKey = here.SectionKey;
                item.MatchIdentity = here.MatchIdentity;

                plan.Items.push_back(std::move(item));
            }
        }

        void PlanLoopbacks(_Inout_ ImportPlan& plan)
        {
            std::vector<std::wstring> namesHere{};

            for (auto const& loopback : plan.Here.Loopbacks)
            {
                for (auto const& name : loopback.EndpointNames)
                {
                    if (!name.empty())
                    {
                        namesHere.push_back(name);
                    }
                }
            }

            auto const anyTaken = [&namesHere](std::vector<std::wstring> const& names)
                {
                    return std::any_of(names.begin(), names.end(), [&namesHere](auto const& name) { return !name.empty() && Contains(namesHere, name); });
                };

            for (auto const& loopback : plan.Package.Loopbacks)
            {
                PlanItem item{};
                item.Id = ItemIdFor(loopback.Basic ? L"loopback:1:" : L"loopback:2:", loopback.AssociationId);
                item.Part = ConfigPart::Loopbacks;
                item.Kind = ItemKind::Loopback;
                item.Name = loopback.BaseName.empty() ? (loopback.EndpointNames.empty() ? std::wstring{} : loopback.EndpointNames.front()) : loopback.BaseName;
                item.EndpointNames = loopback.EndpointNames;
                item.BaseName = loopback.BaseName;
                item.SectionKey = std::wstring{ loopback.Basic ? transportids::BasicLoopback : transportids::Loopback };
                item.EntryKey = loopback.AssociationId;

                auto const same = std::find_if(plan.Here.Loopbacks.begin(), plan.Here.Loopbacks.end(), [&loopback](auto const& here)
                    {
                        if (here.Basic != loopback.Basic || !SameText(here.AssociationId, loopback.AssociationId) ||
                            here.EndpointNames.size() != loopback.EndpointNames.size())
                        {
                            return false;
                        }

                        for (size_t i = 0; i < here.EndpointNames.size(); i++)
                        {
                            if (!SameText(here.EndpointNames[i], loopback.EndpointNames[i]))
                            {
                                return false;
                            }
                        }

                        return true;
                    });

                if (same != plan.Here.Loopbacks.end())
                {
                    item.Status = ItemStatus::AlreadyHere;
                }
                else if (anyTaken(loopback.EndpointNames))
                {
                    item.Status = ItemStatus::NameTaken;

                    for (uint32_t number = 2; number <= MaximumFreeNameAttempts; number++)
                    {
                        std::vector<std::wstring> candidates{};
                        std::wstring freeName{};

                        if (!loopback.BaseName.empty())
                        {
                            freeName = loopback.BaseName + L" " + std::to_wstring(number);

                            if (loopback.Basic)
                            {
                                candidates.push_back(freeName);
                            }
                            else
                            {
                                candidates.push_back(freeName + L" (A)");
                                candidates.push_back(freeName + L" (B)");
                            }
                        }
                        else
                        {
                            for (auto const& name : loopback.EndpointNames)
                            {
                                candidates.push_back(name + L" " + std::to_wstring(number));
                            }

                            freeName = candidates.empty() ? std::wstring{} : candidates.front();
                        }

                        if (!anyTaken(candidates))
                        {
                            item.FreeName = freeName;
                            item.FreeEndpointNames = std::move(candidates);
                            break;
                        }
                    }
                }

                plan.Items.push_back(std::move(item));
            }
        }

        // Free means no host of the same kind here uses it, as a name or as a service name.
        std::wstring FreeVariant(
            _In_ std::wstring const& name,
            _In_ std::vector<std::wstring> const& taken,
            _In_ std::wstring_view const separator)
        {
            if (name.empty() || !Contains(taken, name))
            {
                return name;
            }

            for (uint32_t number = 2; number <= MaximumFreeNameAttempts; number++)
            {
                auto const candidate = name + std::wstring{ separator } + std::to_wstring(number);

                if (!Contains(taken, candidate))
                {
                    return candidate;
                }
            }

            return name;
        }

        void PlanHosts(_Inout_ ImportPlan& plan)
        {
            auto const& otherPc = plan.Source.ComputerName;
            auto const& thisPc = plan.ThisComputerName;

            auto const fromAnotherPc = !otherPc.empty() && !thisPc.empty() && !SameText(otherPc, thisPc);

            for (auto const& host : plan.Package.Hosts)
            {
                std::vector<std::wstring> namesHere{};
                std::vector<std::wstring> serviceNamesHere{};

                for (auto const& here : plan.Here.Hosts)
                {
                    if (here.Kind != host.Kind)
                    {
                        continue;
                    }

                    if (!here.Name.empty())
                    {
                        namesHere.push_back(here.Name);
                    }

                    if (!here.ServiceInstanceName.empty())
                    {
                        serviceNamesHere.push_back(here.ServiceInstanceName);
                    }
                }

                PlanItem item{};
                item.Id = ItemIdFor(std::wstring{ L"host:" } + NetworkLetter(host.Kind) + L":", host.EntryId);
                item.Part = host.Kind == NetworkKind::RtpMidi ? ConfigPart::RtpMidi : ConfigPart::NetworkMidi2;
                item.Kind = ItemKind::Host;
                item.Network = host.Kind;
                item.Name = host.Name;
                item.UsesOtherAdapter = host.UsesOneAdapter;
                item.SectionKey = std::wstring{ host.Kind == NetworkKind::RtpMidi ? transportids::RtpMidi : transportids::NetworkMidi2 };
                item.EntryKey = host.EntryId;

                auto const same = std::find_if(plan.Here.Hosts.begin(), plan.Here.Hosts.end(), [&host](auto const& here)
                    {
                        return here.Kind == host.Kind &&
                            SameText(here.EntryId, host.EntryId) &&
                            SameText(here.Name, host.Name) &&
                            SameText(here.ServiceInstanceName, host.ServiceInstanceName);
                    });

                auto const namedAfterOtherPc = fromAnotherPc &&
                    (ContainsText(host.Name, otherPc) || ContainsText(host.ServiceInstanceName, otherPc));

                if (same != plan.Here.Hosts.end())
                {
                    item.Status = ItemStatus::AlreadyHere;
                }
                else if (namedAfterOtherPc)
                {
                    item.Status = ItemStatus::NamedAfterOtherPc;

                    // An empty name is how a host says "use this PC's name", so it stays empty.
                    auto const renamed = host.Name.empty() ? std::wstring{} : ReplaceText(host.Name, otherPc, thisPc);

                    item.FreeName = FreeVariant(renamed, namesHere, L" ");
                    item.FreeServiceInstanceName = FreeVariant(
                        ReplaceText(host.ServiceInstanceName, otherPc, thisPc), serviceNamesHere, L"-");
                }
                else if ((!host.Name.empty() && Contains(namesHere, host.Name)) ||
                    (!host.ServiceInstanceName.empty() && Contains(serviceNamesHere, host.ServiceInstanceName)))
                {
                    item.Status = ItemStatus::NameTaken;

                    auto taken = namesHere;
                    taken.insert(taken.end(), serviceNamesHere.begin(), serviceNamesHere.end());

                    item.FreeName = FreeVariant(host.Name, taken, L" ");
                    item.FreeServiceInstanceName = FreeVariant(host.ServiceInstanceName, serviceNamesHere, L"-");
                }
                else
                {
                    item.Status = ItemStatus::New;
                }

                plan.Items.push_back(std::move(item));
            }
        }

        void PlanClients(_Inout_ ImportPlan& plan)
        {
            for (auto const& client : plan.Package.Clients)
            {
                PlanItem item{};
                item.Id = ItemIdFor(std::wstring{ L"client:" } + NetworkLetter(client.Kind) + L":", client.EntryId);
                item.Part = client.Kind == NetworkKind::RtpMidi ? ConfigPart::RtpMidi : ConfigPart::NetworkMidi2;
                item.Kind = ItemKind::Client;
                item.Network = client.Kind;
                item.Name = client.DisplayName;
                item.SectionKey = std::wstring{ client.Kind == NetworkKind::RtpMidi ? transportids::RtpMidi : transportids::NetworkMidi2 };
                item.EntryKey = client.EntryId;

                auto const same = std::any_of(plan.Here.Clients.begin(), plan.Here.Clients.end(), [&client](auto const& here)
                    {
                        return here.Kind == client.Kind && !client.Identity.empty() && here.Identity == client.Identity;
                    });

                item.Status = same ? ItemStatus::AlreadyHere : ItemStatus::New;

                plan.Items.push_back(std::move(item));
            }
        }

        void PlanBluetooth(_Inout_ ImportPlan& plan)
        {
            for (auto const& device : plan.Package.BluetoothDevices)
            {
                PlanItem item{};
                item.Id = ItemIdFor(L"bluetooth:", device.DeviceId);
                item.Part = ConfigPart::Bluetooth;
                item.Kind = ItemKind::BluetoothDevice;
                item.Name = device.Name.empty() ? device.DeviceId : device.Name;
                item.SectionKey = std::wstring{ transportids::Bluetooth };
                item.EntryKey = device.DeviceId;

                auto const same = std::any_of(plan.Here.BluetoothDevices.begin(), plan.Here.BluetoothDevices.end(),
                    [&device](auto const& here) { return SameText(here.DeviceId, device.DeviceId); });

                item.Status = same ? ItemStatus::AlreadyHere : ItemStatus::New;

                plan.Items.push_back(std::move(item));
            }
        }

        void PlanOtherSections(_Inout_ ImportPlan& plan)
        {
            for (auto const& section : plan.Package.OtherSections)
            {
                PlanItem item{};
                item.Id = ItemIdFor(L"section:", section);
                item.Part = ConfigPart::OtherSettings;
                item.Kind = ItemKind::OtherSection;
                item.Name = section;
                item.SectionKey = section;
                item.Status = Contains(plan.Here.OtherSections, section) ? ItemStatus::AlreadyHere : ItemStatus::New;

                plan.Items.push_back(std::move(item));
            }
        }

        std::wstring UniqueConfigName(_In_ ConfigEnvironment const& environment, _In_ std::wstring const& name)
        {
            auto const existing = ListConfigurations(environment);

            auto const taken = [&existing](std::wstring const& candidate)
                {
                    return std::any_of(existing.begin(), existing.end(), [&candidate](auto const& file) { return SameText(file.ConfigName, candidate); });
                };

            if (!taken(name))
            {
                return name;
            }

            for (uint32_t number = 2; number <= MaximumFreeNameAttempts; number++)
            {
                auto const candidate = name + L" " + std::to_wstring(number);

                if (!taken(candidate))
                {
                    return candidate;
                }
            }

            return name;
        }
    }

    _Use_decl_annotations_
    bool ImportChoices::IsAddedUnderFreeName(std::wstring const& itemId) const noexcept
    {
        return Contains(AddUnderFreeName, itemId);
    }

    _Use_decl_annotations_
    bool ImportChoices::KeepsOtherPcName(std::wstring const& itemId) const noexcept
    {
        return Contains(KeepOtherPcName, itemId);
    }

    _Use_decl_annotations_
    void ImportChoices::SetAddedUnderFreeName(std::wstring const& itemId, bool const ticked)
    {
        std::erase_if(AddUnderFreeName, [&itemId](auto const& existing) { return existing == itemId; });

        if (ticked)
        {
            AddUnderFreeName.push_back(itemId);
        }
    }

    _Use_decl_annotations_
    void ImportChoices::SetUseThisPcName(std::wstring const& itemId, bool const ticked)
    {
        std::erase_if(KeepOtherPcName, [&itemId](auto const& existing) { return existing == itemId; });

        if (!ticked)
        {
            KeepOtherPcName.push_back(itemId);
        }
    }

    _Use_decl_annotations_
    ConfigResult MakeImportPlan(
        ConfigEnvironment const& environment,
        PackageContents const& contents,
        bool const rtpInstalled,
        ImportPlan& plan) noexcept
    {
        plan = {};

        ConfigResult result{};

        try
        {
            plan.Source = contents.Info;
            plan.IsPlainConfiguration = contents.IsPlainConfiguration;
            plan.ThisComputerName = environment.ComputerName;
            plan.RtpInstalled = rtpInstalled;

            plan.Package = TakeInventory(std::string_view{ contents.ConfigurationBytes });

            if (!plan.Package.IsConfiguration)
            {
                result.Error = ConfigError::NotAConfiguration;
                return result;
            }

            auto const current = environment.ReadCurrentFileName ? environment.ReadCurrentFileName() : std::wstring{};

            if (!current.empty() && IsUsableFileName(current))
            {
                auto const read = ReadWholeFile(environment.ConfigFolder + current, MaximumConfigBytes, plan.PlannedBytes);

                if (read.Succeeded() && LooksLikeConfiguration(plan.PlannedBytes))
                {
                    plan.HasTarget = true;
                    plan.TargetFileName = current;
                    plan.Here = TakeInventory(std::string_view{ plan.PlannedBytes });
                    plan.TargetConfigName = plan.Here.ConfigName.empty() ?
                        current.substr(0, current.size() - (EndsWithText(current, ConfigFileSuffix) ? ConfigFileSuffix.size() : 0)) :
                        plan.Here.ConfigName;
                }
                else if (read.Error != ConfigError::NotFound)
                {
                    // there is a configuration in use, and it can't be read, so nothing is planned
                    // against it rather than against an empty one
                    result.Error = read.Succeeded() ? ConfigError::NotAConfiguration : read.Error;
                    result.SystemError = read.SystemError;
                    plan = {};
                    return result;
                }
                else
                {
                    plan.PlannedBytes.clear();
                }
            }

            auto baseName = CleanConfigName(contents.Info.ConfigName.empty() ? plan.Package.ConfigName : contents.Info.ConfigName);

            if (baseName.empty())
            {
                baseName = L"Configuration";
            }

            auto const& otherPc = contents.Info.ComputerName;

            if (!otherPc.empty() && !SameText(otherPc, environment.ComputerName) && !ContainsText(baseName, otherPc))
            {
                baseName += L" (" + otherPc + L")";
            }

            plan.NewConfigurationName = UniqueConfigName(environment, CleanConfigName(baseName));

            PlanDeviceNames(plan, contents);
            PlanLoopbacks(plan);
            PlanHosts(plan);
            PlanClients(plan);
            PlanBluetooth(plan);
            PlanOtherSections(plan);

            plan.PackageHasSynthMode = plan.Package.HasSynthesizer && !plan.Package.SynthMode.empty();
            plan.PackageSynthMode = plan.Package.SynthMode;
            plan.HereSynthMode = plan.Here.SynthMode;

            plan.PackagePortNaming = contents.PortNamingValue;
            plan.HerePortNaming = environment.ReadPortNaming ? environment.ReadPortNaming() : PortNaming::None;
            plan.PackagePortNamingUsable = plan.PackagePortNaming != PortNaming::Automatic || environment.AutomaticPortNamingAvailable;

            return result;
        }
        catch (...)
        {
            plan = {};
            result.Error = ConfigError::Unexpected;
            return result;
        }
    }

    _Use_decl_annotations_
    ItemOutcome OutcomeFor(PlanItem const& item, ImportChoices const& choices) noexcept
    {
        auto const intoNew = choices.Target == ImportTarget::NewConfiguration;

        switch (item.Kind)
        {
        case ItemKind::DeviceName:
        {
            if (item.Status == ItemStatus::OnlyHere)
            {
                return !intoNew && choices.DeviceNames == DeviceNamesChoice::ReplaceMine ?
                    ItemOutcome::LosesYourName :
                    ItemOutcome::StaysAsItIs;
            }

            if (choices.DeviceNames == DeviceNamesChoice::DontImport)
            {
                return ItemOutcome::NotImported;
            }

            if (intoNew || item.Status == ItemStatus::New)
            {
                return ItemOutcome::WillBeAdded;
            }

            return choices.DeviceNames == DeviceNamesChoice::ReplaceMine || choices.SharedDevices == SharedDevicesChoice::UseThePackages ?
                ItemOutcome::GetsThePackagesName :
                ItemOutcome::KeepsYourName;
        }

        case ItemKind::Loopback:
        case ItemKind::Host:
        case ItemKind::Client:
        case ItemKind::BluetoothDevice:
        {
            auto part = PartChoice::Add;

            switch (item.Part)
            {
            case ConfigPart::Loopbacks:     part = choices.Loopbacks; break;
            case ConfigPart::NetworkMidi2:  part = choices.NetworkMidi2; break;
            case ConfigPart::RtpMidi:       part = choices.RtpMidi; break;
            case ConfigPart::Bluetooth:     part = choices.Bluetooth; break;
            default: break;
            }

            if (part == PartChoice::DontImport)
            {
                return ItemOutcome::NotImported;
            }

            switch (item.Status)
            {
            case ItemStatus::AlreadyHere:
                return intoNew ? ItemOutcome::WillBeAdded : ItemOutcome::StaysAsItIs;

            case ItemStatus::NamedAfterOtherPc:
                return choices.KeepsOtherPcName(item.Id) ? ItemOutcome::KeepsOtherPcName : ItemOutcome::TakesThisPcName;

            case ItemStatus::NameTaken:
                return choices.IsAddedUnderFreeName(item.Id) && !item.FreeName.empty() ?
                    ItemOutcome::AddedUnderNewName :
                    ItemOutcome::LeftOutNameTaken;

            default:
                return ItemOutcome::WillBeAdded;
            }
        }

        case ItemKind::OtherSection:
            return item.Status == ItemStatus::AlreadyHere && !intoNew ? ItemOutcome::StaysAsItIs : ItemOutcome::WillBeAdded;

        default:
            return ItemOutcome::StaysAsItIs;
        }
    }

    _Use_decl_annotations_
    bool IsImported(ItemOutcome const outcome) noexcept
    {
        switch (outcome)
        {
        case ItemOutcome::WillBeAdded:
        case ItemOutcome::GetsThePackagesName:
        case ItemOutcome::AddedUnderNewName:
        case ItemOutcome::TakesThisPcName:
        case ItemOutcome::KeepsOtherPcName:
            return true;
        default:
            return false;
        }
    }

    _Use_decl_annotations_
    bool SynthModeWillChange(ImportPlan const& plan, ImportChoices const& choices) noexcept
    {
        if (choices.OtherSettings != OtherSettingsChoice::UseThePackages || !plan.PackageHasSynthMode)
        {
            return false;
        }

        return choices.Target == ImportTarget::NewConfiguration || !SameText(plan.PackageSynthMode, plan.HereSynthMode);
    }

    _Use_decl_annotations_
    bool PortNamingWillChange(ImportPlan const& plan, ImportChoices const& choices) noexcept
    {
        return choices.OtherSettings == OtherSettingsChoice::UseThePackages &&
            plan.PackagePortNaming != PortNaming::None &&
            plan.PackagePortNamingUsable &&
            plan.PackagePortNaming != plan.HerePortNaming;
    }
}
