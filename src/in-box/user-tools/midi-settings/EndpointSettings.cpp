// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include <windows.h>

#include "EndpointSettings.h"

#include <algorithm>
#include <cwctype>
#include <string_view>

namespace midiapp::customizations
{
    namespace
    {
        constexpr size_t MaximumClearedPortNames = 64;

        // not WIL, which the unit test projects do not have
        void LogFailure(_In_z_ wchar_t const* what) noexcept
        {
            ::OutputDebugStringW(what);
        }

        // the same comparison as midiapp::EndpointIdsMatch, which this file cannot reach
        bool SameEndpoint(_In_ winrt::hstring const& left, _In_ winrt::hstring const& right) noexcept
        {
            return ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
        }

        // the service trims port names, so a box holding only spaces is an empty box
        winrt::hstring Trimmed(_In_ winrt::hstring const& value)
        {
            std::wstring_view text{ value };

            while (!text.empty() && ::iswspace(text.front()))
            {
                text.remove_prefix(1);
            }

            while (!text.empty() && ::iswspace(text.back()))
            {
                text.remove_suffix(1);
            }

            return winrt::hstring{ text };
        }

        std::map<uint8_t, winrt::hstring>& NamesFor(
            _In_ EndpointSettings& settings,
            _In_ midi2enum::Midi1PortFlow const flow) noexcept
        {
            return flow == midi2enum::Midi1PortFlow::MidiMessageSource ? settings.SourcePortNames : settings.DestinationPortNames;
        }

        std::map<uint8_t, winrt::hstring> const& NamesFor(
            _In_ EndpointSettings const& settings,
            _In_ midi2enum::Midi1PortFlow const flow) noexcept
        {
            return flow == midi2enum::Midi1PortFlow::MidiMessageSource ? settings.SourcePortNames : settings.DestinationPortNames;
        }

        bool IsDigits(_In_ std::wstring_view const text) noexcept
        {
            return !text.empty() && std::all_of(text.begin(), text.end(), [](wchar_t const c) { return c >= L'0' && c <= L'9'; });
        }
    }


    _Use_decl_annotations_
    void ClearedPortNames::Saved(
        EndpointSettings const& saved,
        std::vector<ClearedPortName> const& cleared) noexcept
    {
        try
        {
            std::erase_if(m_entries, [&saved, &cleared](Entry const& entry)
                {
                    if (!SameEndpoint(entry.EndpointDeviceId, saved.EndpointDeviceId))
                    {
                        return false;
                    }

                    if (NamesFor(saved, entry.Port.Flow).contains(entry.Port.GroupIndex))
                    {
                        return true;
                    }

                    return std::any_of(cleared.begin(), cleared.end(), [&entry](ClearedPortName const& port)
                        {
                            return port.Flow == entry.Port.Flow && port.GroupIndex == entry.Port.GroupIndex;
                        });
                });

            for (auto const& port : cleared)
            {
                m_entries.push_back({ saved.EndpointDeviceId, port });
            }

            TrimToMaximum();
        }
        catch (...)
        {
            LogFailure(L"Unable to record the cleared port names.\n");
        }
    }


    _Use_decl_annotations_
    void ClearedPortNames::RemoveFrom(EndpointSettings& settings) noexcept
    {
        try
        {
            for (auto entry = m_entries.begin(); entry != m_entries.end();)
            {
                if (!SameEndpoint(entry->EndpointDeviceId, settings.EndpointDeviceId))
                {
                    ++entry;
                    continue;
                }

                auto& names = NamesFor(settings, entry->Port.Flow);

                auto const found = names.find(entry->Port.GroupIndex);

                // anything else there means the service restarted or the port was renamed elsewhere
                if (found == names.end() || found->second != entry->Port.Name)
                {
                    entry = m_entries.erase(entry);
                    continue;
                }

                names.erase(found);
                ++entry;
            }
        }
        catch (...)
        {
            LogFailure(L"Unable to leave out the cleared port names.\n");
        }
    }


    // One line per entry: S or D, group index, endpoint id, then the name, which is the rest.
    std::wstring ClearedPortNames::Serialize() const noexcept
    {
        try
        {
            std::wstring text{};

            for (auto const& entry : m_entries)
            {
                std::wstring_view const id{ entry.EndpointDeviceId };
                std::wstring_view const name{ entry.Port.Name };

                // what could break the line format stays in memory only
                if (id.empty() || name.empty() ||
                    id.find_first_of(L"\t\r\n") != std::wstring_view::npos ||
                    name.find_first_of(L"\r\n") != std::wstring_view::npos)
                {
                    continue;
                }

                text += entry.Port.Flow == midi2enum::Midi1PortFlow::MidiMessageSource ? L'S' : L'D';
                text += L'\t';
                text += std::to_wstring(entry.Port.GroupIndex);
                text += L'\t';
                text += id;
                text += L'\t';
                text += name;
                text += L'\n';
            }

            return text;
        }
        catch (...)
        {
            return {};
        }
    }


    _Use_decl_annotations_
    void ClearedPortNames::Load(std::wstring const& text) noexcept
    {
        try
        {
            m_entries.clear();

            std::wstring_view remaining{ text };

            while (!remaining.empty())
            {
                auto const lineEnd = remaining.find(L'\n');
                auto const line = remaining.substr(0, lineEnd);

                remaining = lineEnd == std::wstring_view::npos ? std::wstring_view{} : remaining.substr(lineEnd + 1);

                auto const first = line.find(L'\t');
                auto const second = first == std::wstring_view::npos ? first : line.find(L'\t', first + 1);
                auto const third = second == std::wstring_view::npos ? second : line.find(L'\t', second + 1);

                if (third == std::wstring_view::npos)
                {
                    continue;
                }

                auto const flow = line.substr(0, first);
                auto const group = line.substr(first + 1, second - first - 1);
                auto const id = line.substr(second + 1, third - second - 1);
                auto const name = line.substr(third + 1);

                if ((flow != L"S" && flow != L"D") || !IsDigits(group) || group.size() > 2 || id.empty() || name.empty())
                {
                    continue;
                }

                auto const groupIndex = group.size() == 1 ?
                    static_cast<uint32_t>(group[0] - L'0') :
                    static_cast<uint32_t>((group[0] - L'0') * 10 + (group[1] - L'0'));

                if (groupIndex > 15)
                {
                    continue;
                }

                m_entries.push_back({
                    winrt::hstring{ id },
                    ClearedPortName{
                        flow == L"S" ? midi2enum::Midi1PortFlow::MidiMessageSource : midi2enum::Midi1PortFlow::MidiMessageDestination,
                        static_cast<uint8_t>(groupIndex),
                        winrt::hstring{ name } } });
            }

            TrimToMaximum();
        }
        catch (...)
        {
            m_entries.clear();
        }
    }


    // the oldest go first, and one that matters again is recorded again by its next save
    void ClearedPortNames::TrimToMaximum() noexcept
    {
        if (m_entries.size() > MaximumClearedPortNames)
        {
            m_entries.erase(m_entries.begin(), m_entries.begin() + (m_entries.size() - MaximumClearedPortNames));
        }
    }


    _Use_decl_annotations_
    void CollectPortNames(
        std::vector<PortNameRow> const& rows,
        midi2enum::Midi1PortFlow const flow,
        std::map<uint8_t, winrt::hstring>& names,
        std::vector<ClearedPortName>& cleared) noexcept
    {
        names.clear();

        try
        {
            for (auto const& row : rows)
            {
                auto const name = Trimmed(row.Text);

                if (!name.empty())
                {
                    names[row.GroupIndex] = name;
                }
                else if (!row.StoredName.empty())
                {
                    cleared.push_back({ flow, row.GroupIndex, row.StoredName });
                }
            }
        }
        catch (...)
        {
            LogFailure(L"Unable to collect the port names.\n");
        }
    }


    _Use_decl_annotations_
    bool HasEditablePortNames(midi2enum::MidiEndpointDeviceInformation const& endpoint) noexcept
    {
        try
        {
            if (endpoint == nullptr)
            {
                return false;
            }

            if (auto const transportInfo = endpoint.GetTransportSuppliedInfo())
            {
                auto const transportCode = transportInfo.TransportCode();

                return transportCode == L"KS" || transportCode == L"KSA";
            }
        }
        catch (...)
        {
            LogFailure(L"Unable to read the endpoint's transport.\n");
        }

        return false;
    }


    _Use_decl_annotations_
    EndpointSettings ReadEndpointSettings(
        midi2enum::MidiEndpointDeviceInformation const& endpoint,
        ClearedPortNames& clearedPortNames) noexcept
    {
        EndpointSettings settings{};

        try
        {
            if (endpoint == nullptr)
            {
                return settings;
            }

            settings.EndpointDeviceId = endpoint.EndpointDeviceId();

            if (auto const userInfo = endpoint.GetUserSuppliedInfo())
            {
                settings.Name = userInfo.Name();
                settings.Description = userInfo.Description();
                settings.ImageFileName = userInfo.ImageFileName();

                settings.RequiresNoteOffTranslation = userInfo.RequiresNoteOffTranslation();
                settings.SupportsMidiPolyphonicExpression = userInfo.SupportsMidiPolyphonicExpression();
                settings.RecommendedControlChangeIntervalMilliseconds = userInfo.RecommendedControlChangeAutomationIntervalMilliseconds();
                settings.OutgoingLatencyTicks = userInfo.CustomMidiOutgoingLatencyTicks();
            }

            settings.Midi1PortNamingApproach = endpoint.Midi1PortNamingApproach();

            if (HasEditablePortNames(endpoint))
            {
                if (auto const table = endpoint.GetNameTable())
                {
                    for (auto const& entry : table)
                    {
                        if (entry.Group() == nullptr || entry.CustomName().empty())
                        {
                            continue;
                        }

                        NamesFor(settings, entry.Flow())[entry.Group().Index()] = entry.CustomName();
                    }
                }
            }

            clearedPortNames.RemoveFrom(settings);
        }
        catch (...)
        {
            LogFailure(L"Unable to read the endpoint's settings.\n");
        }

        return settings;
    }


    _Use_decl_annotations_
    bool HasNonDisplayContent(EndpointSettings const& settings) noexcept
    {
        return
            settings.RequiresNoteOffTranslation ||
            settings.SupportsMidiPolyphonicExpression ||
            settings.RecommendedControlChangeIntervalMilliseconds != 0 ||
            settings.OutgoingLatencyTicks != 0 ||
            settings.Midi1PortNamingApproach != midi2enum::Midi1PortNamingApproach::Default ||
            !settings.SourcePortNames.empty() ||
            !settings.DestinationPortNames.empty();
    }


    _Use_decl_annotations_
    void CopyInto(
        EndpointSettings const& source,
        midi2config::MidiServiceEndpointCustomizationConfig const& destination) noexcept
    {
        try
        {
            if (destination == nullptr)
            {
                return;
            }

            destination.Name(source.Name);
            destination.Description(source.Description);
            destination.ImageFileName(source.ImageFileName);

            destination.RequiresNoteOffTranslation(source.RequiresNoteOffTranslation);
            destination.SupportsMidiPolyphonicExpression(source.SupportsMidiPolyphonicExpression);
            destination.RecommendedControlChangeIntervalMilliseconds(source.RecommendedControlChangeIntervalMilliseconds);
            destination.Midi1PortNamingApproach(source.Midi1PortNamingApproach);

            // UseCustomOutgoingLatency is left unset, so the saved choice is kept.
            destination.OutgoingLatencyTicks(source.OutgoingLatencyTicks);

            for (auto const& [groupIndex, name] : source.SourcePortNames)
            {
                destination.AddMidi1SourcePortCustomName(midi2::MidiGroup{ groupIndex }, name);
            }

            for (auto const& [groupIndex, name] : source.DestinationPortNames)
            {
                destination.AddMidi1DestinationPortCustomName(midi2::MidiGroup{ groupIndex }, name);
            }
        }
        catch (...)
        {
            LogFailure(L"Unable to copy the endpoint's settings.\n");
        }
    }


    _Use_decl_annotations_
    void AddClearedPortNames(
        std::vector<ClearedPortName> const& cleared,
        midi2config::MidiServiceEndpointCustomizationConfig const& destination) noexcept
    {
        try
        {
            for (auto const& port : cleared)
            {
                if (port.Flow == midi2enum::Midi1PortFlow::MidiMessageSource)
                {
                    destination.AddMidi1SourcePortCustomName(midi2::MidiGroup{ port.GroupIndex }, winrt::hstring{});
                }
                else
                {
                    destination.AddMidi1DestinationPortCustomName(midi2::MidiGroup{ port.GroupIndex }, winrt::hstring{});
                }
            }
        }
        catch (...)
        {
            LogFailure(L"Unable to add the cleared port names.\n");
        }
    }
}
