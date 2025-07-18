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

class MidiLoopbackDevice
{
public:
    MidiLoopbackDeviceDefinition DefinitionA;
    MidiLoopbackDeviceDefinition DefinitionB;

//    bool IsFromConfigurationFile{ true };

    void Shutdown()
    {
        if (m_callbackA != nullptr)
        {
            m_callbackA = nullptr;
        }

        if (m_callbackB != nullptr)
        {
            m_callbackB = nullptr;
        }
    }





    void RegisterEndpointA(/*_In_ wil::com_ptr_nothrow<CMidi2LoopbackMidiBidi> endpoint,*/ _In_ wil::com_ptr_nothrow<IMidiCallback> callback)
    {
        //m_bidiA = endpoint;
        m_callbackA = callback;
    }

    void RegisterEndpointB(/*_In_ wil::com_ptr_nothrow<CMidi2LoopbackMidiBidi> endpoint,*/ _In_ wil::com_ptr_nothrow<IMidiCallback> callback)
    {
        //m_bidiB = endpoint;
        m_callbackB = callback;
    }

    HRESULT SendMessageAToB(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG position, _In_ LONGLONG context)
    {
        if (m_callbackB != nullptr)
        {
            return m_callbackB->Callback(message, size, position, context);
        }

        return S_OK;
    }

    HRESULT SendMessageBToA(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG position, _In_ LONGLONG context)
    {
        if (m_callbackA != nullptr)
        {
            return m_callbackA->Callback(message, size, position, context);
        }

        return S_OK;
    }


    ~MidiLoopbackDevice()
    {
        //m_bidiA = nullptr;
        //m_bidiB = nullptr;

        Shutdown();
    }

private:
    // these are needed to enable these two to find each other once opened
    //wil::com_ptr_nothrow<CMidi2LoopbackMidiBidi> m_bidiA{ nullptr };
    //wil::com_ptr_nothrow<CMidi2LoopbackMidiBidi> m_bidiB{ nullptr };

    wil::com_ptr_nothrow<IMidiCallback> m_callbackA{ nullptr };
    wil::com_ptr_nothrow<IMidiCallback> m_callbackB{ nullptr };

};
