// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <utility>

class MidiLoopbackDeviceTable
{
private:
    std::map<std::wstring, std::shared_ptr<MidiLoopbackDevice>> m_devices{};
    mutable std::mutex m_devicesLock;


public:

    std::shared_ptr<MidiLoopbackDevice> GetDevice(std::wstring associationId)
    {
        std::lock_guard<std::mutex> lock{ m_devicesLock };

        auto cleanId = internal::ToLowerTrimmedWStringCopy(associationId);

        if (auto device = m_devices.find(cleanId); device != m_devices.end())
        {
            return device->second;
        }
        else
        {
            return {};
        }
    }

    void SetDevice(std::wstring associationId, MidiLoopbackDevice device)
    {
        std::lock_guard<std::mutex> lock{ m_devicesLock };

        auto cleanId = internal::ToLowerTrimmedWStringCopy(associationId);

        m_devices[cleanId] = std::make_shared<MidiLoopbackDevice>(std::move(device));
    }

    void RemoveDevice(std::wstring associationId)
    {
        std::shared_ptr<MidiLoopbackDevice> deviceToRemove;

        {
            std::lock_guard<std::mutex> lock{ m_devicesLock };

            auto cleanId = internal::ToLowerTrimmedWStringCopy(associationId);

            if (auto device = m_devices.find(cleanId); device != m_devices.end())
            {
                deviceToRemove = device->second;
                m_devices.erase(cleanId);
            }
        }

        if (deviceToRemove)
        {
            deviceToRemove->Shutdown();
        }
    }


    bool IsUniqueIdentifierInUseForLoopbackA(std::wstring uniqueIdentifier)
    {
        std::lock_guard<std::mutex> lock{ m_devicesLock };

        auto cleanId = internal::ToLowerTrimmedWStringCopy(uniqueIdentifier);

        for (auto const& [key, device] : m_devices)
        {
            if (cleanId == internal::ToLowerTrimmedWStringCopy(device->DefinitionA.EndpointUniqueIdentifier))
            {
                return true;
            }
        }

        return false;
    }

    bool IsUniqueIdentifierInUseForLoopbackB(std::wstring uniqueIdentifier)
    {
        std::lock_guard<std::mutex> lock{ m_devicesLock };

        auto cleanId = internal::ToLowerTrimmedWStringCopy(uniqueIdentifier);

        for (auto const& [key, device] : m_devices)
        {
            if (cleanId == internal::ToLowerTrimmedWStringCopy(device->DefinitionB.EndpointUniqueIdentifier))
            {
                return true;
            }
        }

        return false;
    }
};
