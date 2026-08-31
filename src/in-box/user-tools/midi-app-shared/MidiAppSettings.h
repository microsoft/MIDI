// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiapp
{
    enum class AppTheme : int32_t
    {
        System = 0,
        Light = 1,
        Dark = 2
    };

    enum class WindowBackdrop : int32_t
    {
        Solid = 0,
        Mica = 1,
        Acrylic = 2
    };

    // Restore bounds, so a maximized window still reopens at its previous size.
    struct WindowPlacementInfo
    {
        int32_t X{ 0 };
        int32_t Y{ 0 };
        int32_t Width{ 0 };
        int32_t Height{ 0 };
        bool Maximized{ false };
        bool Valid{ false };
    };

    // The settings every Windows MIDI Services tool has: appearance and window placement.
    // Each app derives from this and adds its own, sharing one registry key per tool.
    // Reads and writes never throw; on failure the in-memory value is used.
    class MidiAppSettings
    {
    public:
        virtual ~MidiAppSettings() = default;

        AppTheme Theme() const noexcept { return m_theme; }
        void Theme(AppTheme value) noexcept;

        WindowBackdrop Backdrop() const noexcept { return m_backdrop; }
        void Backdrop(WindowBackdrop value) noexcept;

        bool UseCustomBackgroundColor() const noexcept { return m_useCustomBackgroundColor; }
        void UseCustomBackgroundColor(bool value) noexcept;

        // 0xAARRGGBB
        uint32_t BackgroundColorArgb() const noexcept { return m_backgroundColorArgb; }
        void BackgroundColorArgb(uint32_t value) noexcept;

        bool AlwaysOnTop() const noexcept { return m_alwaysOnTop; }
        void AlwaysOnTop(bool value) noexcept;

        WindowPlacementInfo const& WindowPlacement() const noexcept { return m_windowPlacement; }
        void WindowPlacement(WindowPlacementInfo const& value) noexcept;

        static constexpr uint32_t DefaultBackgroundColorArgb = 0xFF202020;

        static constexpr int32_t MinimumWindowWidth = 640;
        static constexpr int32_t MinimumWindowHeight = 400;

    protected:
        // full HKCU path for this tool, for example
        // Software\Microsoft\Windows MIDI Services\Tools\midi2monitor
        explicit MidiAppSettings(std::wstring settingsKeyPath) noexcept :
            m_settingsKeyPath(std::move(settingsKeyPath))
        {
        }

        // derived Load() must call this first
        void LoadShared() noexcept;

        void WriteDword(std::wstring_view valueName, uint32_t value) const noexcept;
        void WriteString(std::wstring_view valueName, std::wstring const& value) const noexcept;
        uint32_t ReadDword(std::wstring_view valueName, uint32_t defaultValue) const noexcept;
        std::wstring ReadString(std::wstring_view valueName, std::wstring const& defaultValue) const noexcept;

    private:
        std::wstring m_settingsKeyPath{};

        AppTheme m_theme{ AppTheme::System };
        WindowBackdrop m_backdrop{ WindowBackdrop::Solid };
        bool m_useCustomBackgroundColor{ false };
        uint32_t m_backgroundColorArgb{ DefaultBackgroundColorArgb };
        bool m_alwaysOnTop{ false };
        WindowPlacementInfo m_windowPlacement{};
    };
}
