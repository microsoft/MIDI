// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include "AppSettings.h"
#include "Arpeggiator.h"
#include "KeyboardLayout.h"
#include "MidiOutput.h"

namespace winrt::midikeyboard::implementation
{
    // One drawn key. The glow is a separate element so it can be faded out on release
    // without disturbing the key's own brushes.
    struct KeyVisual
    {
        controls::Border Body{ nullptr };
        shapes::Rectangle Glow{ nullptr };
        controls::TextBlock NoteLabel{ nullptr };
        controls::TextBlock ComputerKeyLabel{ nullptr };
        animation::Storyboard FadeOut{ nullptr };
    };

    // A finger, pen or mouse button currently holding a key down.
    struct ActivePointer
    {
        int32_t KeyIndex{ -1 };
        int32_t NoteNumber{ -1 };
        double PressY{ 0.0 };
        uint32_t Pressure{ 0 };
    };

    // One pitch or modulation ribbon. The glow is a composition drop shadow masked to the
    // light's own shape, hosted on an empty element that tracks the light's position.
    struct RibbonVisuals
    {
        controls::Canvas Track{ nullptr };
        shapes::Rectangle Light{ nullptr };
        controls::Border GlowHost{ nullptr };
        winrt::Microsoft::UI::Composition::SpriteVisual GlowVisual{ nullptr };
    };

    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRootSizeChanged(foundation::IInspectable const& sender, xaml::SizeChangedEventArgs const& args);
        void OnRootPreviewKeyDown(foundation::IInspectable const& sender, input::KeyRoutedEventArgs const& args);
        void OnRootPreviewKeyUp(foundation::IInspectable const& sender, input::KeyRoutedEventArgs const& args);

        void OnSettingsToggleClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAlwaysOnTopToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAppearanceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnOctaveDownClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnOctaveUpClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnPanicClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnArpModeChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnArpRateChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnArpBpmChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);

        void OnConnectionModeChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnEndpointSelectionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnGroupSelectionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnChannelSelectionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);

        void OnBaseOctaveChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnOctaveCountChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnTransposeChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnShowNoteNamesChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnShowComputerKeysChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnRibbonPositionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnVelocityModeChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnVelocityMinimumChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnVelocityMaximumChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnFixedVelocityChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnKeyPressureChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnPerNoteControllerChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);

        void OnKeyboardSizeChanged(foundation::IInspectable const& sender, xaml::SizeChangedEventArgs const& args);
        void OnKeyboardPointerPressed(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args);
        void OnKeyboardPointerMoved(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args);
        void OnKeyboardPointerReleased(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args);

        void OnRibbonSizeChanged(foundation::IInspectable const& sender, xaml::SizeChangedEventArgs const& args);
        void OnPitchRibbonPointerPressed(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args);
        void OnPitchRibbonPointerMoved(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args);
        void OnPitchRibbonPointerReleased(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args);
        void OnModRibbonPointerPressed(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args);
        void OnModRibbonPointerMoved(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args);
        void OnModRibbonPointerReleased(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args);

        // called before Activate so the window never appears at the wrong size first
        void RestoreWindowPlacement() noexcept;

    private:
        void InitializeWindowChrome() noexcept;
        void InitializeChoiceLists() noexcept;
        void InitializeControlsFromSettings() noexcept;
        void InitializeSettingsPanelControls() noexcept;
        void ReleaseFlagWhenIdle(bool MainWindow::* flag) noexcept;
        void InitializeBrushes() noexcept;

        void StartEndpointWatcher() noexcept;
        void StopEndpointWatcher() noexcept;
        void RefreshEndpointList() noexcept;
        void RefreshGroupList() noexcept;

        winrt::fire_and_forget ReconnectAsync();
        winrt::fire_and_forget ShutdownAsync();

        void UpdateConnectionDisplay(::midikeyboard::ConnectResult result) noexcept;
        void ShowConnectingState() noexcept;
        void UpdateConnectionModeLayout() noexcept;
        void UpdateVelocityLayout() noexcept;
        void UpdateOctaveDisplay() noexcept;
        void UpdateRibbonLayout() noexcept;

        // ------------------------------------------------------------------ keyboard
        void RebuildKeyboard() noexcept;
        void LayoutKeyboard() noexcept;
        void RefreshKeyGlow(int32_t noteNumber) noexcept;
        void SetKeyGlow(KeyVisual& key, double opacity, bool fade) noexcept;
        int32_t KeyIndexForNote(int32_t noteNumber) const noexcept;

        int32_t FirstNoteNumber() const noexcept;
        int32_t TransposedNote(int32_t noteNumber) const noexcept;

        uint16_t VelocityForKey(::midikeyboard::KeyGeometry const& key, double y) const noexcept;

        // Every input path funnels through these, so a note held by two inputs at once only
        // sounds once and only stops when the last of them lets go.
        void BeginNote(int32_t noteNumber, uint16_t velocity) noexcept;
        void EndNote(int32_t noteNumber) noexcept;
        void EndAllNotes() noexcept;

        void SendNoteOnNow(int32_t noteNumber, uint16_t velocity) noexcept;
        void SendNoteOffNow(int32_t noteNumber) noexcept;
        void SendKeyPressure(int32_t noteNumber, uint32_t pressure) noexcept;

        uint8_t TransmitGroupIndex() const noexcept;
        uint8_t TransmitChannelIndex() const noexcept;

        // ------------------------------------------------------------------ ribbons
        void UpdateRibbonVisual(RibbonVisuals& ribbon, double normalized) noexcept;
        void EnsureRibbonGlow(RibbonVisuals& ribbon) noexcept;
        void UpdateRibbonsFromValues() noexcept;
        double RibbonValueFromPoint(controls::Canvas const& canvas, double y, bool bipolar) const noexcept;
        void ApplyPitchValue(double value, bool send) noexcept;
        void ApplyModValue(double value, bool send) noexcept;

        bool IsTextInputFocused() noexcept;

        midiapp::WindowChrome m_chrome{};

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{ nullptr };

        ::midikeyboard::MidiOutput m_output{};
        ::midikeyboard::Arpeggiator m_arpeggiator{};

        midi2enum::MidiEndpointDeviceWatcher m_watcher{ nullptr };
        winrt::event_token m_watcherAddedToken{};
        winrt::event_token m_watcherRemovedToken{};
        winrt::event_token m_watcherUpdatedToken{};

        collections::IObservableVector<appshared::EndpointChoice> m_endpoints{ nullptr };
        collections::IObservableVector<appshared::NamedChoice> m_groups{ nullptr };
        std::vector<midi2enum::MidiEndpointDeviceInformation> m_endpointDevices{};

        std::vector<KeyVisual> m_keys{};
        std::vector<::midikeyboard::KeyGeometry> m_keyGeometry{};

        // how many separate inputs are holding each note down
        std::array<int32_t, 128> m_noteHoldCount{};

        std::unordered_map<uint32_t, ActivePointer> m_activePointers{};
        std::unordered_map<uint32_t, int32_t> m_computerKeyNotes{};

        int32_t m_arpeggiatorSoundingNote{ -1 };

        double m_pitchValue{ 0.0 };
        double m_modValue{ 0.0 };
        uint32_t m_pitchPointerId{ 0 };
        uint32_t m_modPointerId{ 0 };
        bool m_pitchCaptured{ false };
        bool m_modCaptured{ false };

        RibbonVisuals m_pitchRibbon{};
        RibbonVisuals m_modRibbon{};

        media::Brush m_whiteKeyBrush{ nullptr };
        media::Brush m_blackKeyBrush{ nullptr };
        media::Brush m_keyBorderBrush{ nullptr };
        media::Brush m_glowBrush{ nullptr };
        winrt::Windows::UI::Color m_glowColor{};
        media::Brush m_whiteKeyTextBrush{ nullptr };
        media::Brush m_blackKeyTextBrush{ nullptr };

        bool m_initialized{ false };

        // Starts suppressed. The settings panel is collapsed at launch, and a collapsed control
        // has no template yet, so it raises its change events long after its value is set -
        // late enough to overwrite a saved setting with a coerced default. Its controls are
        // therefore populated the first time the panel is shown, and nothing is written back
        // until that has happened.
        bool m_suppressSettingHandlers{ true };
        bool m_settingsControlsInitialized{ false };

        // the arpeggiator controls live in the always visible strip, so they have their own
        bool m_suppressArpHandlers{ true };

        bool m_startupOptionsApplied{ false };
        bool m_reconnectInProgress{ false };
        bool m_reconnectRequested{ false };
    };
}

namespace winrt::midikeyboard::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
