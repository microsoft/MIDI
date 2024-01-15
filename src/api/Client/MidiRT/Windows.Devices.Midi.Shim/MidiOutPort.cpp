// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "MidiOutPort.h"
#include "MidiOutPort.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    _Use_decl_annotations_
    foundation::IAsyncOperation<midi1::IMidiOutPort> MidiOutPort::FromIdAsync(winrt::hstring deviceId)
    {
        winrt::com_ptr<IMidiAbstraction> serviceAbstraction{};
        winrt::com_ptr<IMidiOut> endpoint{};

        // there's really nothing we need to do async here, but the previous implementation had a lot
        // more plumbing to deal with, including a broker, so it was async. We do the same so that
        // we don't break interface compatibility.
        co_await winrt::resume_background();

        // activate midi service
        if (SUCCEEDED(CoCreateInstance(__uuidof(Midi2MidiSrvAbstraction), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&serviceAbstraction))))
        {
            // instantiate endpoint
            if (SUCCEEDED(serviceAbstraction->Activate(__uuidof(IMidiBiDi), (void**)&endpoint)))
            {
                DWORD mmcssTaskId{};
                ABSTRACTIONCREATIONPARAMS abstractionCreationParams{ MidiDataFormat_ByteStream };

                if (SUCCEEDED(endpoint->Initialize(
                    deviceId.c_str(),
                    &abstractionCreationParams,
                    &mmcssTaskId
                )))
                {
                    auto port = winrt::make_self<winrt::MIDI_ROOT_NAMESPACE_CPP::implementation::MidiOutPort>();

                    port->InternalInitialize(deviceId, endpoint);

                    co_return *port;
                }
            }
        }

        // if we get here, we failed somewhere
        // TODO: Check to see if existing implementation returns nullptr when fails
        co_return nullptr;
    }

    
    winrt::hstring MidiOutPort::GetDeviceSelector()
    {
        // TODO
        return winrt::hstring{};
    }

    MidiOutPort::MidiOutPort()
    {
        // TODO Move this code into the FromIdAsync static method and just return nullptr if it fails
        // TODO: Verify existing implementation returns nullptr on failure



    }


    _Use_decl_annotations_
    void MidiOutPort::SendMessage(midi1::IMidiMessage const& /*midiMessage*/)
    {
        // TODO:

        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiOutPort::SendBuffer(streams::IBuffer const& /*midiData*/)
    {
        // TODO:

        throw hresult_not_implemented();
    }

    void MidiOutPort::Close()
    {
        if (m_endpoint != nullptr)
        {
            m_endpoint->Cleanup();
            m_endpoint = nullptr;
        }
    }
}
