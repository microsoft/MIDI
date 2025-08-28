// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once


class MidiLoopbackDeviceTable
{
private:
    std::map<std::wstring, MidiLoopbackDevice> m_devices{};


public:

    MidiLoopbackDevice* GetDevice(std::wstring associationId)
    {
        auto cleanId = internal::ToLowerTrimmedWStringCopy(associationId);

        if (m_devices.find(cleanId) != m_devices.end())
        {
            return &m_devices[cleanId];
        }
        else
        {
            return nullptr;
        }
    }

    void SetDevice(std::wstring associationId, MidiLoopbackDevice device)
    {
        auto cleanId = internal::ToLowerTrimmedWStringCopy(associationId);

        m_devices[cleanId] = device;
    }

    void RemoveDevice(std::wstring associationId)
    {
        auto cleanId = internal::ToLowerTrimmedWStringCopy(associationId);

        if (auto device = m_devices.find(cleanId); device != m_devices.end())
        {
            device->second.Shutdown();

            m_devices.erase(cleanId);
        }
    }


    bool IsUniqueIdentifierInUseForLoopbackA(std::wstring uniqueIdentifier)
    {
        auto cleanId = internal::ToLowerTrimmedWStringCopy(uniqueIdentifier);

        for (auto const& [key, device] : m_devices)
        {
            if (cleanId == internal::ToLowerTrimmedWStringCopy(device.DefinitionA.EndpointUniqueIdentifier))
            {
                return true;
            }
        }

        return false;
    }

    bool IsUniqueIdentifierInUseForLoopbackB(std::wstring uniqueIdentifier)
    {
        auto cleanId = internal::ToLowerTrimmedWStringCopy(uniqueIdentifier);

        for (auto const& [key, device] : m_devices)
        {
            if (cleanId == internal::ToLowerTrimmedWStringCopy(device.DefinitionB.EndpointUniqueIdentifier))
            {
                return true;
            }
        }

        return false;
    }
};
