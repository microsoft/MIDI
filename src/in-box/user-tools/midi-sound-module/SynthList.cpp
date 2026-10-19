// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Compiled without a precompiled header, so the app and its tests can share it.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shlwapi.h>

// windows.h defines GetObject, which collides with a member of the JSON projection.
#undef GetObject

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

#include <algorithm>
#include <cmath>
#include <memory>

#include "SynthList.h"
#include "SynthDispatcher.h"

namespace json = winrt::Windows::Data::Json;

namespace midisoundmodule
{
    namespace
    {
        constexpr size_t MaximumIdLength = 64;
        constexpr size_t MaximumPathLength = 32767;

        std::wstring Utf8ToWide(_In_ std::string_view text)
        {
            if (text.empty())
            {
                return {};
            }

            auto const required = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);

            if (required <= 0)
            {
                return {};
            }

            std::wstring result(static_cast<size_t>(required), L'\0');

            ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), required);

            return result;
        }

        std::wstring ReadString(_In_ json::JsonObject const& parent, _In_ std::wstring_view key, _In_ size_t maximumLength)
        {
            if (!parent.HasKey(key))
            {
                return {};
            }

            auto const value = parent.GetNamedValue(key);

            if (value == nullptr || value.ValueType() != json::JsonValueType::String)
            {
                return {};
            }

            std::wstring text{ value.GetString() };

            if (text.size() > maximumLength)
            {
                return {};
            }

            return text;
        }

        double ReadNumber(_In_ json::JsonObject const& parent, _In_ std::wstring_view key, _In_ double fallback)
        {
            if (!parent.HasKey(key))
            {
                return fallback;
            }

            auto const value = parent.GetNamedValue(key);

            if (value == nullptr || value.ValueType() != json::JsonValueType::Number)
            {
                return fallback;
            }

            auto const number = value.GetNumber();

            return std::isfinite(number) ? number : fallback;
        }

        bool ReadBoolean(_In_ json::JsonObject const& parent, _In_ std::wstring_view key, _In_ bool fallback)
        {
            if (!parent.HasKey(key))
            {
                return fallback;
            }

            auto const value = parent.GetNamedValue(key);

            if (value == nullptr || value.ValueType() != json::JsonValueType::Boolean)
            {
                return fallback;
            }

            return value.GetBoolean();
        }

        bool IsValidId(_In_ std::wstring const& id) noexcept
        {
            if (id.empty() || id.size() > MaximumIdLength)
            {
                return false;
            }

            return std::all_of(id.begin(), id.end(), [](wchar_t ch)
                {
                    return (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f') || (ch >= L'A' && ch <= L'F') ||
                        ch == L'-' || ch == L'{' || ch == L'}';
                });
        }

        // M2-104-UM: printable ASCII, and no longer than the stream notification can carry.
        bool IsValidProductInstanceId(_In_ std::wstring const& id) noexcept
        {
            if (id.empty() || id.size() > SoundFontSynth::SynthEndpointShape::MaximumProductInstanceIdBytes)
            {
                return false;
            }

            return std::all_of(id.begin(), id.end(), [](wchar_t ch) { return ch >= 0x20 && ch <= 0x7E; });
        }

        std::wstring CleanName(_In_ std::wstring name)
        {
            std::erase_if(name, [](wchar_t ch) { return ch < L' ' || ch == 0x7F; });

            auto const first = name.find_first_not_of(L' ');

            if (first == std::wstring::npos)
            {
                return {};
            }

            name.erase(0, first);
            name.erase(name.find_last_not_of(L' ') + 1);

            if (name.size() > MaximumSynthNameLength)
            {
                name.resize(MaximumSynthNameLength);
            }

            return name;
        }

        SynthList Problem(_In_ SynthListProblem problem, _In_ size_t entry = 0)
        {
            SynthList list{};
            list.Problem = problem;
            list.Entry = entry;

            return list;
        }
    }

    _Use_decl_annotations_
    SynthList ParseSynthList(std::string_view utf8) noexcept
    {
        try
        {
            json::JsonObject root{ nullptr };

            if (!json::JsonObject::TryParse(Utf8ToWide(utf8), root) || root == nullptr)
            {
                return Problem(SynthListProblem::NotJson);
            }

            if (root.HasKey(SynthListKeys::Version))
            {
                auto const version = root.GetNamedValue(SynthListKeys::Version);

                if (version == nullptr || version.ValueType() != json::JsonValueType::Number)
                {
                    return Problem(SynthListProblem::NoSynthList);
                }

                if (version.GetNumber() > SynthListFileVersion)
                {
                    return Problem(SynthListProblem::NewerVersion);
                }
            }

            if (!root.HasKey(SynthListKeys::Synths))
            {
                return Problem(SynthListProblem::NoSynthList);
            }

            auto const listValue = root.GetNamedValue(SynthListKeys::Synths);

            if (listValue == nullptr || listValue.ValueType() != json::JsonValueType::Array)
            {
                return Problem(SynthListProblem::NoSynthList);
            }

            auto const entries = listValue.GetArray();

            if (entries.Size() > MaximumSynths)
            {
                return Problem(SynthListProblem::TooManySynths);
            }

            SynthList list{};
            size_t number{ 0 };

            for (auto const& entry : entries)
            {
                number++;

                if (entry == nullptr || entry.ValueType() != json::JsonValueType::Object)
                {
                    return Problem(SynthListProblem::EntryNotASynth, number);
                }

                auto const item = entry.GetObject();

                SynthDefinition definition{};

                definition.Id = ReadString(item, SynthListKeys::Id, MaximumIdLength);

                if (!IsValidId(definition.Id))
                {
                    return Problem(SynthListProblem::EntryId, number);
                }

                definition.SoundFontPath = ReadString(item, SynthListKeys::SoundFont, MaximumPathLength);

                if (definition.SoundFontPath.empty() || ::PathIsRelativeW(definition.SoundFontPath.c_str()))
                {
                    return Problem(SynthListProblem::EntrySoundBank, number);
                }

                auto const duplicateId = std::any_of(list.Synths.begin(), list.Synths.end(),
                    [&definition](SynthDefinition const& other) { return other.Id == definition.Id; });

                if (duplicateId)
                {
                    return Problem(SynthListProblem::EntryDuplicateId, number);
                }

                definition.Name = CleanName(ReadString(item, SynthListKeys::Name, 1024));

                if (definition.Name.empty())
                {
                    definition.Name = CleanName(std::wstring{ ::PathFindFileNameW(definition.SoundFontPath.c_str()) });
                }

                auto const productInstanceId = ReadString(item, SynthListKeys::ProductInstanceId, 64);

                if (IsValidProductInstanceId(productInstanceId))
                {
                    // Checked above to be printable ASCII, so each character fits a byte.
                    for (auto const character : productInstanceId)
                    {
                        definition.ProductInstanceId.push_back(static_cast<char>(character));
                    }
                }

                // Two synths with one identity would be one endpoint, so a copied entry gets its own.
                auto const duplicateIdentity = std::any_of(list.Synths.begin(), list.Synths.end(),
                    [&definition](SynthDefinition const& other) { return other.ProductInstanceId == definition.ProductInstanceId; });

                if (duplicateIdentity)
                {
                    definition.ProductInstanceId.clear();
                }

                definition.VolumeDb = (std::clamp)(
                    ReadNumber(item, SynthListKeys::VolumeDb, 0.0),
                    SoundFontSynth::Synthesizer::MinimumUserVolumeDb,
                    SoundFontSynth::Synthesizer::MaximumUserVolumeDb);

                definition.Enabled = ReadBoolean(item, SynthListKeys::Enabled, true);

                list.Synths.push_back(std::move(definition));
            }

            return list;
        }
        catch (...)
        {
            // Running out of memory on a file under a megabyte is not something to start empty on.
            return Problem(SynthListProblem::NotJson);
        }
    }
}
