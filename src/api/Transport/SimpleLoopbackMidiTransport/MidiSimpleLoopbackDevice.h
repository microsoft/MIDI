// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

// represents a loopback device. The device has exactly two
// endpoints which are cross-wired to each other

class MidiSimpleLoopbackDevice
{
public:
    MidiSimpleLoopbackDeviceDefinition Definition;

//    bool IsFromConfigurationFile{ true };

    void Shutdown()
    {
        if (m_callback != nullptr)
        {
            m_callback = nullptr;
        }

    }





    void RegisterEndpoint(/*_In_ wil::com_ptr_nothrow<CMidi2LoopbackMidiBidi> endpoint,*/ _In_ wil::com_ptr_nothrow<IMidiCallback> callback)
    {
        //m_bidiA = endpoint;
        m_callback = callback;
    }

    HRESULT SendMessage(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG position, _In_ LONGLONG context)
    {
        if (m_callback != nullptr)
        {
            return m_callback->Callback(MessageOptionFlags_None, message, size, position, context);
        }

        return S_OK;
    }


    ~MidiSimpleLoopbackDevice()
    {
        //m_bidiA = nullptr;
        //m_bidiB = nullptr;

        Shutdown();
    }

private:
    wil::com_ptr_nothrow<IMidiCallback> m_callback{ nullptr };

};
