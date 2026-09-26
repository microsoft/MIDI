// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppSettings.h"

namespace midiglass
{
    namespace
    {
        constexpr wchar_t SettingsKeyPath[] = LR"(Software\Microsoft\Windows MIDI Services\Tools\midiglass)";

        constexpr wchar_t ValueLibrarySort[] = L"LibrarySort";
        constexpr wchar_t ValueLibraryShowsList[] = L"LibraryShowsList";
        constexpr wchar_t ValueKeepAwake[] = L"KeepAwakeWhileRunning";
        constexpr wchar_t ValueRecentLayouts[] = L"RecentLayouts";

        constexpr wchar_t ValueEditorX[] = L"EditorWindowX";
        constexpr wchar_t ValueEditorY[] = L"EditorWindowY";
        constexpr wchar_t ValueEditorWidth[] = L"EditorWindowWidth";
        constexpr wchar_t ValueEditorHeight[] = L"EditorWindowHeight";
        constexpr wchar_t ValueEditorMaximized[] = L"EditorWindowMaximized";

        constexpr wchar_t ValueEditorLeftPane[] = L"EditorLeftPaneWidth";
        constexpr wchar_t ValueEditorInspector[] = L"EditorInspectorWidth";
        constexpr wchar_t ValueEditorMonitor[] = L"EditorMonitorHeight";
        constexpr wchar_t ValueEditorZoom[] = L"EditorZoomPercent";

        // One value holding "path<tab>ticks" a line at a time, rather than a value per layout.
        // A folder of layouts should not leave a registry key nobody can read.
        constexpr wchar_t FieldSeparator = L'\t';
        constexpr wchar_t RecordSeparator = L'\n';

        std::wstring LowerCopy(_In_ std::wstring value) noexcept
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

            return value;
        }
    }

    AppSettings::AppSettings() noexcept :
        midiapp::MidiAppSettings(SettingsKeyPath)
    {
    }

    AppSettings& AppSettings::Current() noexcept
    {
        static AppSettings instance{};
        return instance;
    }

    void AppSettings::Load() noexcept
    {
        LoadShared();

        auto const sort = ReadDword(ValueLibrarySort, 0);

        m_librarySort = sort <= static_cast<uint32_t>(LibrarySort::LastChanged)
            ? static_cast<LibrarySort>(sort)
            : LibrarySort::LastUsed;

        m_libraryShowsList = ReadDword(ValueLibraryShowsList, 0) != 0;
        m_keepAwakeWhileRunning = ReadDword(ValueKeepAwake, 0) != 0;

        m_editorPlacement.X = static_cast<int32_t>(ReadDword(ValueEditorX, 0));
        m_editorPlacement.Y = static_cast<int32_t>(ReadDword(ValueEditorY, 0));
        m_editorPlacement.Width = static_cast<int32_t>(ReadDword(ValueEditorWidth, 0));
        m_editorPlacement.Height = static_cast<int32_t>(ReadDword(ValueEditorHeight, 0));
        m_editorPlacement.Maximized = ReadDword(ValueEditorMaximized, 0) != 0;

        m_editorPlacement.Valid =
            m_editorPlacement.Width >= MinimumWindowWidth &&
            m_editorPlacement.Height >= MinimumWindowHeight;

        m_editorLeftPaneWidth = static_cast<int32_t>(ReadDword(ValueEditorLeftPane, 0));
        m_editorInspectorWidth = static_cast<int32_t>(ReadDword(ValueEditorInspector, 0));
        m_editorMonitorHeight = static_cast<int32_t>(ReadDword(ValueEditorMonitor, 0));
        m_editorZoomPercent = static_cast<int32_t>(ReadDword(ValueEditorZoom, 0));

        LoadRecentLayouts();
    }

    _Use_decl_annotations_
    void AppSettings::EditorPlacement(WindowPlacementInfo const& value) noexcept
    {
        m_editorPlacement = value;
        m_editorPlacement.Valid = true;

        WriteDword(ValueEditorX, static_cast<uint32_t>(value.X));
        WriteDword(ValueEditorY, static_cast<uint32_t>(value.Y));
        WriteDword(ValueEditorWidth, static_cast<uint32_t>(value.Width));
        WriteDword(ValueEditorHeight, static_cast<uint32_t>(value.Height));
        WriteDword(ValueEditorMaximized, value.Maximized ? 1u : 0u);
    }

    _Use_decl_annotations_
    void AppSettings::EditorPaneSizes(
        int32_t leftPaneWidth,
        int32_t inspectorWidth,
        int32_t monitorHeight,
        int32_t zoomPercent) noexcept
    {
        m_editorLeftPaneWidth = leftPaneWidth;
        m_editorInspectorWidth = inspectorWidth;
        m_editorMonitorHeight = monitorHeight;
        m_editorZoomPercent = zoomPercent;

        WriteDword(ValueEditorLeftPane, static_cast<uint32_t>(leftPaneWidth));
        WriteDword(ValueEditorInspector, static_cast<uint32_t>(inspectorWidth));
        WriteDword(ValueEditorMonitor, static_cast<uint32_t>(monitorHeight));
        WriteDword(ValueEditorZoom, static_cast<uint32_t>(zoomPercent));
    }

    _Use_decl_annotations_
    void AppSettings::LibrarySortOrder(LibrarySort value) noexcept
    {
        m_librarySort = value;

        WriteDword(ValueLibrarySort, static_cast<uint32_t>(value));
    }

    _Use_decl_annotations_
    void AppSettings::LibraryShowsList(bool value) noexcept
    {
        m_libraryShowsList = value;

        WriteDword(ValueLibraryShowsList, value ? 1u : 0u);
    }

    _Use_decl_annotations_
    void AppSettings::KeepAwakeWhileRunning(bool value) noexcept
    {
        m_keepAwakeWhileRunning = value;

        WriteDword(ValueKeepAwake, value ? 1u : 0u);
    }

    void AppSettings::LoadRecentLayouts() noexcept
    {
        m_recentLayouts.clear();

        try
        {
            auto const stored = ReadString(ValueRecentLayouts, {});

            size_t position{ 0 };

            while (position < stored.size() && m_recentLayouts.size() < MaximumRecentLayouts)
            {
                auto const end = stored.find(RecordSeparator, position);
                auto const line = stored.substr(
                    position, end == std::wstring::npos ? std::wstring::npos : end - position);

                auto const tab = line.find(FieldSeparator);

                if (tab != std::wstring::npos && tab > 0)
                {
                    auto const path = line.substr(0, tab);
                    auto const ticks = _wcstoi64(line.c_str() + tab + 1, nullptr, 10);

                    if (ticks > 0)
                    {
                        m_recentLayouts.emplace_back(path, ticks);
                    }
                }

                if (end == std::wstring::npos)
                {
                    break;
                }

                position = end + 1;
            }
        }
        catch (...)
        {
            m_recentLayouts.clear();
        }
    }

    void AppSettings::SaveRecentLayouts() const noexcept
    {
        try
        {
            std::wstring stored{};

            for (auto const& [path, ticks] : m_recentLayouts)
            {
                stored += path;
                stored += FieldSeparator;
                stored += std::to_wstring(ticks);
                stored += RecordSeparator;
            }

            WriteString(ValueRecentLayouts, stored);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    int64_t AppSettings::LayoutLastUsed(std::wstring const& layoutFilePath) const noexcept
    {
        auto const key = LowerCopy(layoutFilePath);

        for (auto const& [path, ticks] : m_recentLayouts)
        {
            if (path == key)
            {
                return ticks;
            }
        }

        return 0;
    }

    _Use_decl_annotations_
    void AppSettings::RecordLayoutUse(std::wstring const& layoutFilePath) noexcept
    {
        if (layoutFilePath.empty())
        {
            return;
        }

        auto const key = LowerCopy(layoutFilePath);

        FILETIME now{};
        ::GetSystemTimeAsFileTime(&now);

        auto const ticks = static_cast<int64_t>(
            (static_cast<uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime);

        m_recentLayouts.erase(
            std::remove_if(m_recentLayouts.begin(), m_recentLayouts.end(),
                [&key](auto const& entry) { return entry.first == key; }),
            m_recentLayouts.end());

        m_recentLayouts.insert(m_recentLayouts.begin(), { key, ticks });

        if (m_recentLayouts.size() > MaximumRecentLayouts)
        {
            m_recentLayouts.resize(MaximumRecentLayouts);
        }

        SaveRecentLayouts();
    }
}
