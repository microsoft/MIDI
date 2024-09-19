// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once


class MidiNetworkDeviceTable
{
public:

    MidiNetworkDevice* GetDevice(std::wstring associationId)
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

    void SetDevice(std::wstring associationId, MidiNetworkDevice device)
    {
        m_devices[associationId] = device;
    }

    void RemoveDevice(std::wstring associationId)
    {
        if (auto device = m_devices.find(associationId); device != m_devices.end())
        {
            device->second.Cleanup();

            m_devices.erase(associationId);
        }
    }


private:
    std::map<std::wstring, MidiNetworkDevice> m_devices{};

};
