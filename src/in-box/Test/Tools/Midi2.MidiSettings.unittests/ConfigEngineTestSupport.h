// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// UNSUPPORTED - DO NOT COPY THIS APPROACH. These tests drive code that reads and writes the
// Windows MIDI Services configuration file directly, in a temporary folder of their own. Microsoft
// does not support direct manipulation of that file by anything other than the in-box MIDI tools.
// Applications and third-party tools must use the Windows MIDI Services API
// (Windows.Devices.Midi2.ServiceConfig) instead.

#include <windows.h>

#include <memory>
#include <string>

#include "ConfigEngine.h"

namespace configtests
{
    namespace engine = ::midisettings::configengine;
    namespace json = ::winrt::Windows::Data::Json;

    // A folder under %TEMP% holding config\, backups\, pictures\ and temp\, removed with everything
    // in it when the test ends. Nothing a test does reaches the real configurations or the registry.
    class TestFolder
    {
    public:
        TestFolder();
        ~TestFolder();

        TestFolder(TestFolder const&) = delete;
        TestFolder& operator=(TestFolder const&) = delete;

        // each with a trailing separator
        std::wstring Config() const { return m_root + L"config\\"; }
        std::wstring Backups() const { return m_root + L"backups\\"; }
        std::wstring Pictures() const { return m_root + L"pictures\\"; }
        std::wstring Temporary() const { return m_root + L"temp\\"; }
        std::wstring Root() const { return m_root; }

    private:
        std::wstring m_root{};
    };

    // What the registry would hold.
    struct FakeSystem
    {
        std::wstring CurrentFileName{};
        engine::PortNaming Naming{ engine::PortNaming::ClassicCompatible };
        bool AutomaticAvailable{ true };
        uint32_t PortNamingWrites{ 0 };
    };

    // Pictures are stored the way the picture chooser stores them: "ep-" and the source's name, an
    // identical file reused, and a different one with the same name given a "(1)".
    engine::ConfigEnvironment MakeEnvironment(
        _In_ TestFolder const& folder,
        _In_ std::shared_ptr<FakeSystem> const& system,
        _In_ std::wstring const& computerName = L"STAGE-LAPTOP");

    std::string ReadFileBytes(_In_ std::wstring const& path);
    void WriteFileBytes(_In_ std::wstring const& path, _In_ std::string const& bytes);
    bool FileExists(_In_ std::wstring const& path);

    std::string Utf8(_In_ std::wstring const& text);

    // A configuration whose endpointTransportPluginSettings object is the given JSON text.
    std::string MakeConfig(_In_ std::wstring const& configName, _In_ std::wstring const& settingsJson);

    json::JsonObject Parse(_In_ std::string const& bytes);

    // endpointTransportPluginSettings > section
    json::JsonObject Section(_In_ json::JsonObject const& document, _In_ std::wstring const& sectionKey);

    // a small picture file's bytes, different for each seed
    std::string PictureBytes(_In_ uint8_t seed);
}
