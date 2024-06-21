#pragma once

#include "MainWindow.g.h"

namespace winrt::virtual_device_app_winui::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow()
        {
            // Xaml objects should not call InitializeComponent during construction.
            // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent
        }

        //void myButton_Click(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        void OnMidiMessageReceived(_In_ midi2::IMidiMessageReceivedEventSource const&, _In_ midi2::MidiMessageReceivedEventArgs const& args);
        void OnStreamConfigurationRequestReceived(_In_ virt::MidiVirtualDevice const&, _In_ virt::MidiStreamConfigRequestReceivedEventArgs const& args);

        virt::MidiVirtualDeviceCreationConfig DefineDevice();
        void StartVirtualDevice();


        midi2::MidiSession m_session{ nullptr };
        midi2::MidiEndpointConnection m_connection{ nullptr };
        virt::MidiVirtualDevice m_virtualDevice{ nullptr };

    };
}

namespace winrt::virtual_device_app_winui::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
