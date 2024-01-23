// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiVirtualEndpointDevice.h"
#include "MidiVirtualEndpointDevice.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{


    _Use_decl_annotations_
    void MidiVirtualEndpointDevice::UpdateFunctionBlock(midi2::MidiFunctionBlock const& block) noexcept
    {
        UNREFERENCED_PARAMETER(block);

        //throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiVirtualEndpointDevice::UpdateEndpointName(winrt::hstring const& name) noexcept
    {
        UNREFERENCED_PARAMETER(name);

        //throw hresult_not_implemented();
    }


    _Use_decl_annotations_
    void MidiVirtualEndpointDevice::Initialize(midi2::IMidiEndpointConnectionSource const& endpointConnection) noexcept
    {
        OutputDebugString(__FUNCTION__ L"");

        m_endpointConnection = endpointConnection.as<midi2::MidiEndpointConnection>();
    }

    void MidiVirtualEndpointDevice::OnEndpointConnectionOpened() noexcept
    {
        OutputDebugString(__FUNCTION__ L"");

        //throw hresult_not_implemented();
    }

    void MidiVirtualEndpointDevice::Cleanup() noexcept
    {
        //throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiVirtualEndpointDevice::SendFunctionBlockInfoNotificationMessage(midi2::MidiFunctionBlock const& fb) noexcept
    {
        OutputDebugString(__FUNCTION__ L"");

        auto functionBlockNotification = midi2::MidiStreamMessageBuilder::BuildFunctionBlockInfoNotificationMessage(
            0,
            true,
            fb.Number(),
            fb.UIHint(),
            fb.Midi10Connection(),
            fb.Direction(),
            fb.FirstGroupIndex(),
            fb.GroupCount(),
            fb.MidiCIMessageVersionFormat(),
            fb.MaxSystemExclusive8Streams()
        );

        // TODO: Log if failed
        m_endpointConnection.SendMessagePacket(functionBlockNotification);
    }

    _Use_decl_annotations_
    void MidiVirtualEndpointDevice::SendFunctionBlockNameNotificationMessages(midi2::MidiFunctionBlock const& fb) noexcept
    {
        OutputDebugString(__FUNCTION__ L"");

        if (fb.Name() == L"") return;

        auto nameMessages = midi2::MidiStreamMessageBuilder::BuildFunctionBlockNameNotificationMessages(
            0, 
            fb.Number(),
            fb.Name()
        );

        for (uint32_t i = 0; i < nameMessages.Size(); i++)
        {
            // TODO: Log if failed
            m_endpointConnection.SendMessagePacket(nameMessages.GetAt(i));
        }

    }

    _Use_decl_annotations_
    void MidiVirtualEndpointDevice::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& args,
        bool& skipFurtherListeners,
        bool& skipMainMessageReceivedEvent)  noexcept
    {
        OutputDebugString(__FUNCTION__ L"");

        bool handled = false;

        if (args.MessageType() == MidiMessageType::Stream128)
        {
            MidiMessage128 message{};
            if (args.FillMessage128(message))
            {

                // if a endpoint discovery request, handle it with the data we have
                if (internal::MessageIsEndpointDiscoveryRequest(message.Word0()))
                {
                    uint8_t filterFlags = internal::GetEndpointDiscoveryMessageFilterFlagsFromSecondWord(message.Word1());

                    if (internal::EndpointDiscoveryFilterRequestsEndpointInfoNotification(filterFlags))
                    {
                        // send endpoint info notification

                        auto notification = midi2::MidiStreamMessageBuilder::BuildEndpointInformationNotificationMessage(
                            0, 
                            MIDI_PREFERRED_UMP_VERSION_MAJOR,
                            MIDI_PREFERRED_UMP_VERSION_MINOR,
                            m_areFunctionBlocksStatic,
                            (uint8_t)m_functionBlocks.Size(),
                            true,   // TODO: Pull from properties supports midi 2.0
                            true,   // TODO: pull from properties supports midi 1.0
                            false,  // todo: pull from default JR timestamp handling
                            false   // todo: pull from jr timestamp handling
                        );

                        m_endpointConnection.SendMessagePacket(notification);
                    }

                    if (internal::EndpointDiscoveryFilterRequestsDeviceIdentityNotification(filterFlags))
                    {
                        // send device identity notification
                    }

                    if (internal::EndpointDiscoveryFilterRequestsEndpointNameNotification(filterFlags))
                    {
                        // send endpoint name notification messages
                    }

                    if (internal::EndpointDiscoveryFilterRequestsProductInstanceIdNotification(filterFlags))
                    {
                        // send product instance id notification messages
                        
                    }

                    if (internal::EndpointDiscoveryFilterRequestsStreamConfigurationNotification(filterFlags))
                    {
                        // send stream configuration message
                    }
                }
                else if (internal::MessageIsFunctionBlockDiscoveryRequest(message.Word0()))
                {
                    uint8_t filterFlags = internal::GetFunctionBlockDiscoveryMessageFilterFlagsFromFirstWord(message.Word0());

                    bool requestInfo = internal::FunctionBlockDiscoveryFilterRequestsInfoNotification(filterFlags);
                    bool requestName = internal::FunctionBlockDiscoveryFilterRequestsNameNotification(filterFlags);

                    uint8_t fbNumber = internal::GetFunctionBlockNumberFromFunctionBlockDiscoveryRequestFirstWord(message.Word0());

                    if (fbNumber == MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_REQUEST_ALL_FUNCTION_BLOCKS)
                    {
                        // send all function blocks

                        for (uint8_t i = 0; i < (uint8_t)m_functionBlocks.Size(); i++)
                        {
                            if (requestInfo) SendFunctionBlockInfoNotificationMessage(m_functionBlocks.Lookup(i));
                            if (requestName) SendFunctionBlockNameNotificationMessages(m_functionBlocks.Lookup(i));
                        }
                    }
                    else
                    {
                        // send single function block
                           
                        if (m_functionBlocks.HasKey(fbNumber))
                        {
                            auto fb = m_functionBlocks.Lookup(fbNumber);

                            if (requestInfo) SendFunctionBlockInfoNotificationMessage(fb);
                            if (requestName) SendFunctionBlockNameNotificationMessages(fb);
                        }
                        else
                        {
                            // invalid fb number request
                            handled = false;
                        }
                    }

                }
                else if (internal::MessageIsEndpointDiscoveryRequest(message.Word0()))
                {
                }
                else
                {
                    // something else
                }



            }
            else
            {
                // something went wrong filling this message type
            }
            

        }
        else
        {
            // not a stream message. Ignore


        }


        if (handled && SuppressHandledMessages())
        {
            skipFurtherListeners = true;
            skipMainMessageReceivedEvent = true;
        }
        else
        {
            skipFurtherListeners = false;
            skipMainMessageReceivedEvent = false;
        }

    }




    _Use_decl_annotations_
    void MidiVirtualEndpointDevice::InternalSetDeviceDefinition(
        _In_ midi2::MidiVirtualEndpointDeviceDefinition definition)
    {
        try
        {
            OutputDebugString(__FUNCTION__ L"");

            // populate all the views, properties, blocks, etc.

            m_areFunctionBlocksStatic = definition.AreFunctionBlocksStatic();

            for (uint8_t i = 0; i < definition.FunctionBlocks().Size(); i++)
            {
                auto fb = definition.FunctionBlocks().GetAt(i);

                // this is required, so we enforce it here
                fb.Number(i);

                // TODO: Set the fb as read-only


                 
                // add the block
                m_functionBlocks.Insert(i, fb);
            }

            m_endpointName = definition.EndpointName();
            m_endpointProductInstanceId = definition.EndpointProductInstanceId();


            // TODO: All the other stuff we'll want to report on




        }
        catch (...)
        {
            // todo log
        }

    }

}
