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

    MidiLoopbackDevice* GetDevice(_In_ std::wstring const& associationId)
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

    void SetDevice(_In_ std::wstring const& associationId, _In_ MidiLoopbackDevice const& device)
    {
        auto cleanId = internal::ToLowerTrimmedWStringCopy(associationId);

        m_devices[cleanId] = device;
    }

    void RemoveDevice(_In_ std::wstring const& associationId)
    {
        auto cleanId = internal::ToLowerTrimmedWStringCopy(associationId);

        if (auto device = m_devices.find(cleanId); device != m_devices.end())
        {
            device->second.Shutdown();

            m_devices.erase(cleanId);
        }
    }


    bool IsUniqueIdentifierInUseForLoopbackA(_In_ std::wstring const& uniqueIdentifier)
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

    bool IsUniqueIdentifierInUseForLoopbackB(_In_ std::wstring const& uniqueIdentifier)
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



    std::vector<MidiLoopbackDevice> GetDeviceListSnapshot()
    {
        std::vector<MidiLoopbackDevice> results;

        // lock so no adds/removes happen while building the list
        // UPDATE THIS with the Loopback locking mechanism that was added through security PR
//        auto lock = m_devicesLock.lock_shared();

        for (auto const& [key, device] : m_devices)
        {
            results.push_back(device);
        }

        return results;
    }


};
