// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "RecentSequences.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <algorithm>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

namespace json = winrt::Windows::Data::Json;

namespace midisequencer
{
    namespace
    {
        constexpr wchar_t KeyPath[] = L"path";
        constexpr wchar_t KeyUsedAt[] = L"usedAt";
        constexpr wchar_t KeyPinned[] = L"pinned";

        // A setting that lists far more than it should is read only this far.
        constexpr size_t MaximumEntriesRead = 4 * (MaximumRecentSequences + MaximumPinnedSequences);

        // Seconds since 1970 up to the year 33658, which a JSON number holds exactly.
        constexpr double LatestUsedAt = 1.0e12;

        std::wstring Normalized(std::wstring_view path)
        {
            std::wstring text{ path };
            std::replace(text.begin(), text.end(), L'/', L'\\');
            return text;
        }

        // Drops the oldest sequences that aren't pinned, past the limit. The list is newest first.
        void Trim(std::vector<RecentSequence>& list)
        {
            size_t unpinned{ 0 };

            std::erase_if(list, [&unpinned](RecentSequence const& entry)
            {
                return !entry.Pinned && ++unpinned > MaximumRecentSequences;
            });
        }

        void NewestFirst(std::vector<RecentSequence>& list)
        {
            std::stable_sort(list.begin(), list.end(), [](RecentSequence const& left, RecentSequence const& right)
            {
                return left.UsedAt > right.UsedAt;
            });
        }
    }

    _Use_decl_annotations_
    bool SameSequencePath(std::wstring_view left, std::wstring_view right) noexcept
    {
        try
        {
            if (left.size() != right.size() || left.empty())
            {
                return false;
            }

            auto const a = Normalized(left);
            auto const b = Normalized(right);

            return ::CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()), b.c_str(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    void RememberSequence(std::vector<RecentSequence>& list, std::wstring const& path, int64_t now)
    {
        if (path.empty() || path.size() > MaximumRecentPathLength)
        {
            return;
        }

        auto pinned{ false };

        std::erase_if(list, [&](RecentSequence const& entry)
        {
            if (!SameSequencePath(entry.Path, path))
            {
                return false;
            }

            pinned = pinned || entry.Pinned;
            return true;
        });

        list.insert(list.begin(), RecentSequence{ path, now, pinned });
        Trim(list);
    }

    _Use_decl_annotations_
    bool PinSequence(std::vector<RecentSequence>& list, std::wstring_view path, bool pinned)
    {
        auto const found = std::find_if(list.begin(), list.end(), [path](RecentSequence const& entry)
        {
            return SameSequencePath(entry.Path, path);
        });

        if (found == list.end())
        {
            return false;
        }

        if (pinned && !found->Pinned)
        {
            auto const count = std::count_if(list.begin(), list.end(), [](RecentSequence const& entry) { return entry.Pinned; });

            if (static_cast<size_t>(count) >= MaximumPinnedSequences)
            {
                return false;
            }
        }

        // An unpinned sequence stays where it is until the next one opened pushes it out, so it
        // doesn't vanish the moment it's unpinned.
        found->Pinned = pinned;
        return true;
    }

    _Use_decl_annotations_
    void ForgetSequence(std::vector<RecentSequence>& list, std::wstring_view path)
    {
        std::erase_if(list, [path](RecentSequence const& entry)
        {
            return SameSequencePath(entry.Path, path);
        });
    }

    _Use_decl_annotations_
    std::vector<RecentSequence> RecentSequencesForDisplay(std::vector<RecentSequence> const& list)
    {
        auto shown = list;
        NewestFirst(shown);

        std::stable_partition(shown.begin(), shown.end(), [](RecentSequence const& entry) { return entry.Pinned; });
        return shown;
    }

    _Use_decl_annotations_
    std::vector<RecentSequence> ReadRecentSequences(std::wstring const& text) noexcept
    {
        std::vector<RecentSequence> list{};

        try
        {
            json::JsonArray array{ nullptr };

            if (text.empty() || !json::JsonArray::TryParse(text, array))
            {
                return list;
            }

            size_t read{ 0 };

            for (auto const& value : array)
            {
                if (++read > MaximumEntriesRead)
                {
                    break;
                }

                if (value.ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                auto const o = value.GetObject();

                if (!o.HasKey(KeyPath) || o.Lookup(KeyPath).ValueType() != json::JsonValueType::String)
                {
                    continue;
                }

                std::wstring path{ o.Lookup(KeyPath).GetString() };

                if (path.empty() || path.size() > MaximumRecentPathLength)
                {
                    continue;
                }

                int64_t usedAt{ 0 };

                if (o.HasKey(KeyUsedAt) && o.Lookup(KeyUsedAt).ValueType() == json::JsonValueType::Number)
                {
                    auto const number = o.Lookup(KeyUsedAt).GetNumber();

                    // NaN fails both comparisons and stays 0.
                    if (number > 0 && number <= LatestUsedAt)
                    {
                        usedAt = static_cast<int64_t>(number);
                    }
                }

                auto const pinned = o.HasKey(KeyPinned) && o.Lookup(KeyPinned).ValueType() == json::JsonValueType::Boolean && o.Lookup(KeyPinned).GetBoolean();

                auto const existing = std::find_if(list.begin(), list.end(), [&path](RecentSequence const& entry)
                {
                    return SameSequencePath(entry.Path, path);
                });

                if (existing != list.end())
                {
                    if (usedAt > existing->UsedAt)
                    {
                        existing->Path = path;
                        existing->UsedAt = usedAt;
                    }

                    existing->Pinned = existing->Pinned || pinned;
                    continue;
                }

                list.push_back(RecentSequence{ std::move(path), usedAt, pinned });
            }

            NewestFirst(list);

            // Past their limits, pinned ones count as unpinned, and the oldest unpinned ones go.
            size_t pinnedCount{ 0 };

            for (auto& entry : list)
            {
                if (entry.Pinned && ++pinnedCount > MaximumPinnedSequences)
                {
                    entry.Pinned = false;
                }
            }

            Trim(list);
        }
        catch (...)
        {
            list.clear();
        }

        return list;
    }

    _Use_decl_annotations_
    std::wstring WriteRecentSequences(std::vector<RecentSequence> const& list)
    {
        json::JsonArray array{};

        for (auto const& entry : list)
        {
            if (entry.Path.empty() || entry.Path.size() > MaximumRecentPathLength)
            {
                continue;
            }

            json::JsonObject o{};
            o.SetNamedValue(KeyPath, json::JsonValue::CreateStringValue(entry.Path));
            o.SetNamedValue(KeyUsedAt, json::JsonValue::CreateNumberValue(static_cast<double>(std::clamp<int64_t>(entry.UsedAt, 0, static_cast<int64_t>(LatestUsedAt)))));

            if (entry.Pinned)
            {
                o.SetNamedValue(KeyPinned, json::JsonValue::CreateBooleanValue(true));
            }

            array.Append(o);
        }

        return std::wstring{ array.Stringify() };
    }
}
