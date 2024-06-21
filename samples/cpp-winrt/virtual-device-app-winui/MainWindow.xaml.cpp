#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::virtual_device_app_winui::implementation
{

    void MainWindow::StartVirtualDevice()
    {
        if (!init::MidiServicesInitializer::EnsureServiceAvailable()) return; // return if unable to start up the service

        // TODO: Bootstrap SDK



        // define our virtual device
        auto creationConfig = DefineDevice();

        // create the session. The name here is just convenience.
        m_session = midi2::MidiSession::Create(creationConfig.Name());

        // return if unable to create session
        if (m_session == nullptr) return;

        // create the virtual device, so we can get the endpoint device id to connect to
        m_virtualDevice = virt::MidiVirtualDeviceManager::CreateVirtualDevice(creationConfig);

        // return if unable to create virtual device
        if (m_virtualDevice == nullptr) return;

        // create our device-side connection
        m_connection = m_session.CreateEndpointConnection(m_virtualDevice.DeviceEndpointDeviceId());

        // add the virtual device as a message processing plugin so it receives the messages
        m_connection.AddMessageProcessingPlugin(m_virtualDevice);

        // wire up the stream configuration request received handler
        auto streamEventToken = m_virtualDevice.StreamConfigRequestReceived({ this, &MainWindow::OnStreamConfigurationRequestReceived });

        // wire up the message received handler on the connection itself
        auto messageEventToken = m_connection.MessageReceived({ this, &MainWindow::OnMidiMessageReceived });

        if (m_connection.Open())
        {
         //   AppWindow().Title(creationConfig.Name() + L": Connected");
        }
        else
        {
         //   AppWindow().Title(creationConfig.Name() + L": (no connection)");
        }

    }

    _Use_decl_annotations_
    void MainWindow::OnMidiMessageReceived(midi2::IMidiMessageReceivedEventSource const&, midi2::MidiMessageReceivedEventArgs const& /*args*/)
    {
        // handle incoming messages as they arrive

    }

    _Use_decl_annotations_
    void MainWindow::OnStreamConfigurationRequestReceived(virt::MidiVirtualDevice const&, virt::MidiStreamConfigRequestReceivedEventArgs const& /*args*/)
    {
        // respond with an appropriate stream configuration response
    }



    virt::MidiVirtualDeviceCreationConfig MainWindow::DefineDevice()
    {
        // some of these values may seem redundant, but for physical devices
        // they are all sourced from different locations, and we want virtual
        // devices to behave like physical devices.

        winrt::hstring userSuppliedName = L"Pad Controller App";
        winrt::hstring userSuppliedDescription = L"My favorite demo app for Windows MIDI Services";

        winrt::hstring transportSuppliedName = L"Contoso Pad Controller 1.0";
        winrt::hstring transportSuppliedDescription = L"A sample app-to-app MIDI virtual device";
        winrt::hstring transportSuppliedManufacturerName = L"Constoso, Inc.";

        winrt::hstring endpointSuppliedName = transportSuppliedName;


        midi2::MidiDeclaredEndpointInfo declaredEndpointInfo{ };
        declaredEndpointInfo.Name = endpointSuppliedName;
        declaredEndpointInfo.ProductInstanceId = L"PMB_APP2_3263827";
        declaredEndpointInfo.SpecificationVersionMajor = 1; // see latest MIDI 2 UMP spec
        declaredEndpointInfo.SpecificationVersionMinor = 1; // see latest MIDI 2 UMP spec
        declaredEndpointInfo.SupportsMidi10Protocol = true;
        declaredEndpointInfo.SupportsMidi20Protocol = true;
        declaredEndpointInfo.SupportsReceivingJitterReductionTimestamps = false;
        declaredEndpointInfo.SupportsSendingJitterReductionTimestamps = false;
        declaredEndpointInfo.HasStaticFunctionBlocks = true;

        midi2::MidiDeclaredDeviceIdentity declaredDeviceIdentity{ };
        // todo: set any device identity values if you want. This is optional

        midi2::MidiEndpointUserSuppliedInfo userSuppliedInfo{ };
        userSuppliedInfo.Name = userSuppliedName;           // for names, this will bubble to the top in priority
        userSuppliedInfo.Description = userSuppliedDescription;


        virt::MidiVirtualDeviceCreationConfig config(
            transportSuppliedName,                          // this could be a different "transport-supplied" name value here
            transportSuppliedDescription,                   // transport-supplied description
            transportSuppliedManufacturerName,              // transport-supplied company name
            declaredEndpointInfo,                           // for endpoint discovery
            declaredDeviceIdentity,                         // for endpoint discovery
            userSuppliedInfo
        );

        // Function blocks. The MIDI 2 UMP specification covers the meanings
        // of these values
        midi2::MidiFunctionBlock block1{};
        block1.Number(0);
        block1.Name(L"Pads Output");
        block1.IsActive(true);
        block1.UIHint(midi2::MidiFunctionBlockUIHint::Sender);
        block1.FirstGroupIndex(0);
        block1.GroupCount(1);
        block1.Direction(midi2::MidiFunctionBlockDirection::Bidirectional);
        block1.RepresentsMidi10Connection(midi2::MidiFunctionBlockRepresentsMidi10Connection::Not10);
        block1.MaxSystemExclusive8Streams(0);
        block1.MidiCIMessageVersionFormat(0);

        config.FunctionBlocks().Append(block1);

        midi2::MidiFunctionBlock block2{};
        block2.Number(1);
        block2.Name(L"A Function Block");
        block2.IsActive(true);
        block2.UIHint(midi2::MidiFunctionBlockUIHint::Sender);
        block2.FirstGroupIndex(1);
        block2.GroupCount(2);
        block2.Direction(midi2::MidiFunctionBlockDirection::Bidirectional);
        block2.RepresentsMidi10Connection(midi2::MidiFunctionBlockRepresentsMidi10Connection::Not10);
        block2.MaxSystemExclusive8Streams(0);
        block2.MidiCIMessageVersionFormat(0);

        config.FunctionBlocks().Append(block2);

        return config;
    }




}
