#include "stdafx.h"

#include <mmsystem.h>
#include <vector>

#pragma comment(lib, "winmm.lib")


#include "DiscordReproTest.h"

#include <iostream>

struct EndpointInfoBase
{
    winrt::hstring endpointId;
    winrt::hstring endpointDeviceId;
    midi2::Enumeration::MidiGroupTerminalBlock gtb { nullptr };
    midi2::MidiGroup group{ nullptr };
};

struct InputEndpointInfo : EndpointInfoBase
{};

struct OutputEndpointInfo : EndpointInfoBase
{};

struct InputEndpointHandle
{
    InputEndpointInfo* info;
    MidiGroupEndpointListener groupListener{ nullptr };
    winrt::event_token revokeOnGroupListener;
};

struct OutputEndpointHandle
{
    OutputEndpointInfo* info;
};

struct EndpointDevicesInfo
{
    std::vector<InputEndpointInfo*> inputEndpointsInfo{};
    std::vector<OutputEndpointInfo*> outputEndpointsInfo{};
};

struct SessionHandle
{
    const wchar_t* name{ nullptr };

    midi2::Enumeration::MidiEndpointDeviceWatcher watcher{ nullptr };
    winrt::event_token revokeOnWatcherDeviceRemoved;
    winrt::event_token revokeOnWatcherDeviceAdded;
    winrt::event_token revokeOnWatcherEnumerationCompleted;
    std::atomic<char> initialEnumerationCompleted{ 0 };

    midi2::MidiSession session{ nullptr };
    std::mutex endpointDevicesLock;
    std::unordered_map<std::wstring, EndpointDevicesInfo> endpointDevicesById;

    //std::unordered_map<std::wstring, midi2::MidiEndpointConnection> endpointConnectionsById;
    winrt::Windows::Foundation::Collections::IMap<winrt::hstring, midi2::MidiEndpointConnection> endpointConnectionsById = 
        winrt::single_threaded_map<winrt::hstring, midi2::MidiEndpointConnection>();
};


void DiscordReproTests::TestMaximRepro()
{
    winrt::init_apartment();

    // Create MIDI session and set up watcher.
    // Watcher events are used to handle endpoints cache

    SessionHandle* sessionHandle = new SessionHandle();

    sessionHandle->session = midi2::MidiSession::Create(L"Sample session");
    sessionHandle->watcher = midi2::Enumeration::MidiEndpointDeviceWatcher::Create(
        midi2::Enumeration::MidiEndpointDeviceInformationFilters::AllStandardEndpoints);

    auto OnWatcherDeviceAdded = [sessionHandle](midi2::Enumeration::MidiEndpointDeviceWatcher const&, midi2::Enumeration::MidiEndpointDeviceInformationAddedEventArgs const& args)
        {
            std::wcout << L"- Device Added: " << args.AddedDevice().Name().c_str() << std::endl;

            std::lock_guard<std::mutex> lock(sessionHandle->endpointDevicesLock);

            EndpointDevicesInfo endpointDevicesInfo;

            auto endpointId = args.AddedDevice().EndpointDeviceId();
            auto endpointInformation = midi2::Enumeration::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(endpointId);
            auto groupTerminalBlocks = endpointInformation.GetGroupTerminalBlocks();

            for (auto const& gtb : groupTerminalBlocks)
            {
                auto const group = gtb.FirstGroup();
                auto direction = gtb.Direction();

                if (direction == midi2::Enumeration::MidiGroupTerminalBlockDirection::BlockInput)
                {
                    OutputEndpointInfo* outputEndpointInfo = new OutputEndpointInfo();
                    outputEndpointInfo->endpointDeviceId = endpointId;
                    outputEndpointInfo->gtb = gtb;
                    outputEndpointInfo->group = group;
                    endpointDevicesInfo.outputEndpointsInfo.push_back(outputEndpointInfo);
                }
                else if (direction == midi2::Enumeration::MidiGroupTerminalBlockDirection::BlockOutput)
                {
                    InputEndpointInfo* inputEndpointInfo = new InputEndpointInfo();
                    inputEndpointInfo->endpointDeviceId = endpointId;
                    inputEndpointInfo->gtb = gtb;
                    inputEndpointInfo->group = group;
                    endpointDevicesInfo.inputEndpointsInfo.push_back(inputEndpointInfo);
                }
            }

            std::wstring endpointKey = endpointId.c_str();

            sessionHandle->endpointDevicesById[endpointKey] = std::move(endpointDevicesInfo);
        };

    auto OnWatcherDeviceRemoved = [sessionHandle](
        midi2::Enumeration::MidiEndpointDeviceWatcher const&, 
        midi2::Enumeration::MidiEndpointDeviceInformationRemovedEventArgs const& args)
        {
            if (sessionHandle->initialEnumerationCompleted.load() == 0)
                return;

            std::lock_guard<std::mutex> lock(sessionHandle->endpointDevicesLock);

            auto endpointId = args.EndpointDeviceId();
            std::wstring endpointKey = endpointId.c_str();

            EndpointDevicesInfo endpointDevicesInfo;

            auto it = sessionHandle->endpointDevicesById.find(endpointKey);
            if (it == sessionHandle->endpointDevicesById.end())
                return;

            endpointDevicesInfo = it->second;
            sessionHandle->endpointDevicesById.erase(it);
        };

    auto OnWatcherEnumerationCompleted = [sessionHandle](
        midi2::Enumeration::MidiEndpointDeviceWatcher const&, 
        winrt::Windows::Foundation::IInspectable const&)
        {
            sessionHandle->initialEnumerationCompleted.store(1);
        };

    sessionHandle->revokeOnWatcherEnumerationCompleted = sessionHandle->watcher.EnumerationCompleted(OnWatcherEnumerationCompleted);
    sessionHandle->revokeOnWatcherDeviceRemoved = sessionHandle->watcher.Removed(OnWatcherDeviceRemoved);
    sessionHandle->revokeOnWatcherDeviceAdded = sessionHandle->watcher.Added(OnWatcherDeviceAdded);

    // Create basic loopback endpoint (LAST VIRT)

    auto virtualDeviceName = L"VIRT LAST";

    winrt::hstring uniqueId = winrt::to_hstring(winrt::Windows::Foundation::GuidHelper::CreateNewGuid());
    auto associationId = winrt::Windows::Foundation::GuidHelper::CreateNewGuid();

    basicLoopback::MidiBasicLoopbackEndpointDefinition definition;
    definition.Name(virtualDeviceName);
    definition.UniqueId(uniqueId);

    basicLoopback::MidiBasicLoopbackCreationConfig config(associationId, definition);
    basicLoopback::MidiBasicLoopbackManager::CreateTransientLoopback(config);

    // Get input (LAST VIRT) for the created basic loopback.
    // Input info = endpoint ID + GTB + group

    sessionHandle->watcher.Start();



    std::cout << "Search inputs. " << std::endl;

    InputEndpointInfo* lastVirtInputEndpoint = nullptr;

    while (sessionHandle->initialEnumerationCompleted.load() == 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    {
        std::lock_guard<std::mutex> lock(sessionHandle->endpointDevicesLock);

        bool found = false;

        for (const auto& pair : sessionHandle->endpointDevicesById)
        {
            if (found)
                break;

            const EndpointDevicesInfo& endpointDevicesInfo = pair.second;

            for (auto* inputEndpointInfo : endpointDevicesInfo.inputEndpointsInfo)
            {
                if (inputEndpointInfo->gtb.Name() == virtualDeviceName)
                {
                    lastVirtInputEndpoint = inputEndpointInfo;
                    found = true;
                    break;
                }
            }
        }
    }

    // [Service crash trigger] Open connection on endpoint corresponding to input,
    // and add group listener which is turned off

    VERIFY_IS_NOT_NULL(lastVirtInputEndpoint);
    std::wcout << L"Opening connection on input endpoint: " << lastVirtInputEndpoint->endpointDeviceId.c_str() << std::endl;

    InputEndpointHandle* inputEndpointHandle = new InputEndpointHandle();
    inputEndpointHandle->info = lastVirtInputEndpoint;

    {
        std::lock_guard<std::mutex> lock(sessionHandle->endpointDevicesLock);

        inputEndpointHandle->groupListener = MidiGroupEndpointListener();
        inputEndpointHandle->groupListener.IncludedGroups().Append(midi2::MidiGroup(static_cast<uint8_t>(lastVirtInputEndpoint->group.Index())));
        inputEndpointHandle->groupListener.PreventCallingFurtherListeners(false);
        inputEndpointHandle->groupListener.PreventFiringMainMessageReceivedEvent(true);
        inputEndpointHandle->groupListener.PluginName(L"Group listener " + std::to_wstring(lastVirtInputEndpoint->group.Index()));
        inputEndpointHandle->groupListener.IsEnabled(false);

        auto MessageReceivedHandler = [](midi2::IMidiMessageReceivedEventSource const& sender, midi2::MidiMessageReceivedEventArgs const& args)
            {
                UNREFERENCED_PARAMETER(sender);
                UNREFERENCED_PARAMETER(args);
            };

        inputEndpointHandle->revokeOnGroupListener = inputEndpointHandle->groupListener.MessageReceived(MessageReceivedHandler);

        if (!sessionHandle->endpointConnectionsById.HasKey(lastVirtInputEndpoint->endpointId))
        {
            std::cout << "Creating and opening connection. " << std::endl;

            auto connection = sessionHandle->session.CreateEndpointConnection(lastVirtInputEndpoint->endpointDeviceId);

            VERIFY_IS_NOT_NULL(connection);

            sessionHandle->endpointConnectionsById.Insert(lastVirtInputEndpoint->endpointId, connection);
            connection.Open();
        }

        std::cout << "Adding message processing plugin. " << std::endl;

        sessionHandle->endpointConnectionsById.Lookup(lastVirtInputEndpoint->endpointId).AddMessageProcessingPlugin(inputEndpointHandle->groupListener);
    }

    // Enable group listener

    std::cout << "Enabling listener. " << std::endl;
    inputEndpointHandle->groupListener.IsEnabled(true);

    // Delete basic loopback

    std::cout << "Removing basic loopback. " << std::endl;
    basicLoopback::MidiBasicLoopbackManager::RemoveTransientLoopback(
        basicLoopback::MidiBasicLoopbackRemovalConfig{ associationId });

    // Find output which matches MIDI A port (from loopMIDI).
    // Output info = endpoint ID + GTB + group

    std::cout << "Search outputs. " << std::endl;

    OutputEndpointInfo* midiAOutputEndpoint = nullptr;

    {
        std::lock_guard<std::mutex> lock(sessionHandle->endpointDevicesLock);

        bool found = false;

        for (const auto& pair : sessionHandle->endpointDevicesById)
        {
            if (found)
                break;

            const EndpointDevicesInfo& endpointDevicesInfo = pair.second;

            for (auto* outputEndpointInfo : endpointDevicesInfo.outputEndpointsInfo)
            {
                std::wcout << L"Output endpoint: " << outputEndpointInfo->gtb.Name().c_str() << std::endl;

                // this exposes hte problem where an index can get added to the gtb name for some reason
                // so changed to "starts_with" to avoid this issue.
                if (outputEndpointInfo->gtb.Name().starts_with(L"MIDI A"))
                {
                    midiAOutputEndpoint = outputEndpointInfo;
                    found = true;
                    break;
                }
            }
        }

        VERIFY_IS_TRUE(found);
    }

    // Open connection on endpoint corresponding to output

    std::cout << "Open output. " << std::endl;

    VERIFY_IS_NOT_NULL(midiAOutputEndpoint);

    OutputEndpointHandle* outputEndpointHandle = new OutputEndpointHandle();
    outputEndpointHandle->info = midiAOutputEndpoint;

    {
        std::lock_guard<std::mutex> lock(sessionHandle->endpointDevicesLock);

        std::cout << "Looking for endpoint. " << std::endl;

        if (!sessionHandle->endpointConnectionsById.HasKey(midiAOutputEndpoint->endpointId))
        {
            std::cout << "Creating and opening connection. " << std::endl;

            auto connection = sessionHandle->session.CreateEndpointConnection(midiAOutputEndpoint->endpointDeviceId);
            VERIFY_IS_NOT_NULL(connection);

            sessionHandle->endpointConnectionsById.Insert(midiAOutputEndpoint->endpointId, connection);
            connection.Open();
        }
    }

    // [Service crash trigger] Send Note On to MIDI A

    std::cout << "Send note on to MIDI A " << std::endl;

    {
        std::lock_guard<std::mutex> lock(sessionHandle->endpointDevicesLock);

        auto connection = sessionHandle->endpointConnectionsById.Lookup(outputEndpointHandle->info->endpointId);
        VERIFY_IS_NOT_NULL(connection);

        auto ump = messages::MidiMessageConverter::ConvertMidi1Message(
            midi2::MidiClock::TimestampConstantSendImmediately(),
            outputEndpointHandle->info->group,
            0x90,
            0x45,
            0x7F);

        connection.SendSingleMessagePacket(ump);
    }

    //

    std::wcout << L"Press Enter to exit..." << std::endl;
    std::cin.get();

}