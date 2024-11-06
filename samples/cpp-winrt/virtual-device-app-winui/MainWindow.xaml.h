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

            if (!init::MidiServicesInitializer::EnsureServiceAvailable())
            {
                // In your application, you may decide it is appropriate to fall back to an older MIDI API
                //Console.WriteLine("Windows MIDI Services is not available");
            }
            else
            {
                // bootstrap the SDK runtime. Should check the return result here
                init::MidiServicesInitializer::InitializeDesktopAppSdkRuntime();

                StartVirtualDevice();

                uint8_t notes[] { 50, 52, 53, 55, 57, 58, 60, 62, 64, 65, 67, 69, 70, 72, 74, 76 };

                for (auto const noteNumber : notes)
                {
                    winrt::virtual_device_app_winui::Note note{};

                    note.NoteNumber(noteNumber);
                    note.Connection(m_connection);
                    note.GroupIndex(0);
                    note.ChannelIndex(0);

                    m_notes.Append(note);
                }
            }
        }

        //void myButton_Click(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);

        winrt::Windows::Foundation::Collections::IVector<IInspectable> Notes() { return m_notes; }

    private:
        void OnMidiMessageReceived(_In_ midi2::IMidiMessageReceivedEventSource const&, _In_ midi2::MidiMessageReceivedEventArgs const& args);
        void OnStreamConfigurationRequestReceived(_In_ virt::MidiVirtualDevice const&, _In_ virt::MidiStreamConfigRequestReceivedEventArgs const& args);

        virt::MidiVirtualDeviceCreationConfig DefineDevice();
        void StartVirtualDevice();


        midi2::MidiSession m_session{ nullptr };
        midi2::MidiEndpointConnection m_connection{ nullptr };
        virt::MidiVirtualDevice m_virtualDevice{ nullptr };

        // have to use IInspectable for binding. But this is a vector of the Note type
        winrt::Windows::Foundation::Collections::IVector<IInspectable> m_notes{
            winrt::single_threaded_vector<IInspectable>() };

    };
}

namespace winrt::virtual_device_app_winui::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
