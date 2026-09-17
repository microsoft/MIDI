// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <fstream>

namespace SequencingTests
{
    // The .mid files live beside the test binary. They are checked in and are written by
    // test-files\generate-test-files.ps1, which is where to go if a count below ever needs to
    // change.
    inline std::wstring TestFilePath(_In_ std::wstring const& name)
    {
        // This has to be the directory of the test DLL, not of the process. The process is the
        // test host, and it lives somewhere else entirely.
        HMODULE thisModule{ nullptr };

        if (!::GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&TestFilePath),
            &thisModule))
        {
            return name;
        }

        wchar_t modulePath[MAX_PATH]{};

        if (::GetModuleFileNameW(thisModule, modulePath, ARRAYSIZE(modulePath)) == 0)
        {
            return name;
        }

        std::filesystem::path path{ modulePath };

        path = path.parent_path() / L"sequencing-test-files" / name;

        return path.wstring();
    }

    inline winrt::Windows::Devices::Midi2::Utilities::Files::MidiFileReadResult ReadTestFile(
        _In_ std::wstring const& name)
    {
        auto const path = TestFilePath(name);

        auto file = storage::StorageFile::GetFileFromPathAsync(winrt::hstring{ path }).get();

        return winrt::Windows::Devices::Midi2::Utilities::Files::MidiStandardFileReader::ReadFromFileAsync(file).get();
    }

    inline winrt::Windows::Devices::Midi2::Utilities::Sequencing::MidiSequence ReadTestSequence(
        _In_ std::wstring const& name)
    {
        auto const result = ReadTestFile(name);

        VERIFY_IS_TRUE(result != nullptr);
        VERIFY_IS_TRUE(result.Succeeded());

        return result.Sequence();
    }

    // Reading straight from bytes, for the cases that build a file in memory.
    inline winrt::Windows::Devices::Midi2::Utilities::Files::MidiFileReadResult ReadBytes(
        _In_ std::vector<uint8_t> const& bytes)
    {
        streams::InMemoryRandomAccessStream stream{};

        streams::DataWriter writer{ stream };

        writer.WriteBytes(winrt::array_view<uint8_t const>{ bytes });
        writer.StoreAsync().get();
        writer.FlushAsync().get();
        writer.DetachStream();

        stream.Seek(0);

        return winrt::Windows::Devices::Midi2::Utilities::Files::MidiStandardFileReader::ReadAsync(stream).get();
    }

    inline std::vector<uint8_t> ReadTestFileBytes(_In_ std::wstring const& name)
    {
        std::ifstream stream{ TestFilePath(name), std::ios::binary };

        std::istreambuf_iterator<char> first{ stream };
        std::istreambuf_iterator<char> last{};

        return std::vector<uint8_t>(first, last);
    }

    // Group zero, spelled so the byte overload is chosen rather than an int.
    inline winrt::Windows::Devices::Midi2::MidiGroup FirstGroup()
    {
        return winrt::Windows::Devices::Midi2::MidiGroup{ static_cast<uint8_t>(0) };
    }

    // A loopback endpoint is enough to prove what reaches the wire, and unlike the synth it makes
    // no sound, so the test runner stays quiet.
    inline winrt::hstring LoopbackAEndpointId()
    {
        return winrt::hstring{ MIDI_DIAGNOSTICS_LOOPBACK_BIDI_ID_A };
    }

    inline winrt::hstring LoopbackBEndpointId()
    {
        return winrt::hstring{ MIDI_DIAGNOSTICS_LOOPBACK_BIDI_ID_B };
    }
}
