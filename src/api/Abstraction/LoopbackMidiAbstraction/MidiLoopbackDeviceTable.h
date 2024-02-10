// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once


class MidiEndpointTable
{
public:

    MidiLoopbackDevice* GetDevice(std::wstring associationId)
    {
        if (m_devices.find(associationId) != m_devices.end())
        {
            return &m_devices[associationId];
        }
        else
        {
            return nullptr;
        }
    }

    void SetDevice(std::wstring associationId, MidiLoopbackDevice device)
    {
        m_devices[associationId] = device;
    }


private:
    std::map<std::wstring, MidiLoopbackDevice> m_devices{};

};
