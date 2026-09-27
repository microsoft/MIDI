// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// PROTOTYPE test helper.
//
//   patch-oracle <file.midipatch.json> <connection index> <word> [<word> ...]
//
// Reads a patch file's filter and transform with MIDI Patchbay's own FilterFromJson and
// TransformFromJson, runs each one-word message through the app's own Allows and Apply, and
// prints "<in> <out>" or "<in> dropped". So a draft is judged by the code that will run it, not by
// the code that wrote it.

#include "pch.h"

#include "MessageFilter.h"
#include "MessageTransform.h"

#include <cstdio>
#include <fstream>
#include <sstream>

// The summaries need this to link. Nothing here shows a string to anybody.
namespace midipatchbay::resources
{
    winrt::hstring GetString(std::wstring_view resourceKey) noexcept
    {
        return winrt::hstring{ resourceKey };
    }
}

namespace
{
    std::wstring ReadUtf8File(wchar_t const* path)
    {
        std::ifstream stream{ path, std::ios::binary };
        std::stringstream buffer{};
        buffer << stream.rdbuf();

        auto const bytes = buffer.str();
        auto const required = ::MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);

        std::wstring text(static_cast<size_t>(required > 0 ? required : 0), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), text.data(), required);

        return text;
    }

    json::JsonObject ObjectOrNull(json::JsonObject const& parent, wchar_t const* key)
    {
        if (parent.HasKey(key) && parent.GetNamedValue(key).ValueType() == json::JsonValueType::Object)
        {
            return parent.GetNamedObject(key);
        }

        return nullptr;
    }
}

int __cdecl wmain(int argc, wchar_t** argv)
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    if (argc < 4)
    {
        std::fwprintf(stderr, L"usage: patch-oracle <file> <connection> <word>...\n");
        return 2;
    }

    json::JsonObject root{ nullptr };

    if (!json::JsonObject::TryParse(ReadUtf8File(argv[1]), root))
    {
        std::fwprintf(stderr, L"not JSON\n");
        return 3;
    }

    auto const connections = root.GetNamedArray(L"connections");
    auto const index = static_cast<uint32_t>(_wtoi(argv[2]));

    if (index >= connections.Size())
    {
        std::fwprintf(stderr, L"no connection %u\n", index);
        return 4;
    }

    auto const connection = connections.GetObjectAt(index);

    // Exactly what PatchStore::LoadFile does with these two objects.
    auto const filter = midipatchbay::FilterFromJson(ObjectOrNull(connection, L"filter"));
    auto const transform = midipatchbay::TransformFromJson(ObjectOrNull(connection, L"transform"));

    for (int i = 3; i < argc; i++)
    {
        uint32_t word = static_cast<uint32_t>(std::wcstoul(argv[i], nullptr, 16));

        if (!filter.Allows(&word, 1))
        {
            std::wprintf(L"%08X dropped\n", static_cast<unsigned>(std::wcstoul(argv[i], nullptr, 16)));
            continue;
        }

        auto const in = word;
        transform.Apply(&word, 1);

        std::wprintf(L"%08X %08X\n", in, word);
    }

    return 0;
}
