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

    void SetDevice(_In_ std::wstring const& associationId, _In_ MidiLoopbackDevice const& device)
    {
        std::lock_guard<std::mutex> lock{ m_devicesLock };

        auto cleanId = internal::ToLowerTrimmedWStringCopy(associationId);

        m_devices[cleanId] = std::make_shared<MidiLoopbackDevice>(device);
    }

    void RemoveDevice(_In_ std::wstring const& associationId)
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
            if (Feature_Servicing_MIDI2LoopbackAddRemoveReliability::IsEnabled())
            {
                deviceToRemove->ShutdownWithLock();
            }
            else
            {
                deviceToRemove->Shutdown();
            }
        }
    }


    bool IsUniqueIdentifierInUseForLoopbackA(_In_ std::wstring const& uniqueIdentifier)
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

    bool IsUniqueIdentifierInUseForLoopbackB(_In_ std::wstring const& uniqueIdentifier)
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

    // Net-new for Feature_Servicing_MIDI2LoopbackAddRemoveReliability. The service removes
    // endpoints by the start of their instance id, so removing one of two overlapping loopbacks
    // would take the other with it.
    bool IsUniqueIdentifierOverlappingAnotherLoopbackA(_In_ std::wstring const& uniqueIdentifier)
    {
        std::lock_guard<std::mutex> lock{ m_devicesLock };

        auto const cleanId = internal::ToLowerTrimmedWStringCopy(uniqueIdentifier);

        for (auto const& [key, device] : m_devices)
        {
            auto const otherId = internal::ToLowerTrimmedWStringCopy(device->DefinitionA.EndpointUniqueIdentifier);

            if (cleanId.starts_with(otherId) || otherId.starts_with(cleanId))
            {
                return true;
            }
        }

        return false;
    }

    // Net-new for Feature_Servicing_MIDI2LoopbackAddRemoveReliability.
    bool IsUniqueIdentifierOverlappingAnotherLoopbackB(_In_ std::wstring const& uniqueIdentifier)
    {
        std::lock_guard<std::mutex> lock{ m_devicesLock };

        auto const cleanId = internal::ToLowerTrimmedWStringCopy(uniqueIdentifier);

        for (auto const& [key, device] : m_devices)
        {
            auto const otherId = internal::ToLowerTrimmedWStringCopy(device->DefinitionB.EndpointUniqueIdentifier);

            if (cleanId.starts_with(otherId) || otherId.starts_with(cleanId))
            {
                return true;
            }
        }

        return false;
    }

    // Either side of any pair, because the name is what an application sees and it has no way
    // to tell two endpoints called the same thing apart.
    bool IsEndpointNameInUse(_In_ std::wstring const& endpointName)
    {
        std::lock_guard<std::mutex> lock{ m_devicesLock };

        auto cleanName = internal::ToLowerTrimmedWStringCopy(endpointName);

        for (auto const& [key, device] : m_devices)
        {
            if (cleanName == internal::ToLowerTrimmedWStringCopy(device->DefinitionA.EndpointName) ||
                cleanName == internal::ToLowerTrimmedWStringCopy(device->DefinitionB.EndpointName))
            {
                return true;
            }
        }

        return false;
    }



    std::vector< std::shared_ptr<MidiLoopbackDevice>> GetDeviceListSnapshot()
    {
        std::lock_guard<std::mutex> lock{ m_devicesLock };
        std::vector< std::shared_ptr<MidiLoopbackDevice>> results;

        for (auto const& [key, device] : m_devices)
        {
            results.push_back(device);
        }

        return results;
    }


    // Net-new for Feature_Servicing_MIDI2LoopbackHardening. Endpoint A is visible to apps before
    // B is created, and the pair only arrives here after both exist. An endpoint opened in that
    // moment used to be connected to nothing for as long as it stayed open. Now its registration
    // waits here and is applied when the pair is added. Checked and recorded under one lock, so the
    // pair cannot be added in between.
    std::shared_ptr<MidiLoopbackDevice> GetDeviceOrDeferRegistration(
        _In_ std::wstring const& associationId,
        _In_ bool const isSideA,
        _In_ wil::com_ptr_nothrow<IMidiCallback> const& callback)
    {
        std::lock_guard<std::mutex> lock{ m_devicesLock };

        auto cleanId = internal::ToLowerTrimmedWStringCopy(associationId);

        if (auto device = m_devices.find(cleanId); device != m_devices.end())
        {
            return device->second;
        }

        m_pendingRegistrations.push_back(PendingRegistration{ cleanId, isSideA, callback });

        return {};
    }

    // Net-new for Feature_Servicing_MIDI2LoopbackHardening. For an endpoint closed, or refused,
    // before its pair was added.
    void RemovePendingRegistrations(_In_opt_ IMidiCallback* const callback)
    {
        std::vector<PendingRegistration> removed{};

        {
            std::lock_guard<std::mutex> lock{ m_devicesLock };

            for (auto entry = m_pendingRegistrations.begin(); entry != m_pendingRegistrations.end();)
            {
                if (entry->Callback.get() == callback)
                {
                    removed.push_back(std::move(*entry));
                    entry = m_pendingRegistrations.erase(entry);
                }
                else
                {
                    ++entry;
                }
            }
        }

        // released here, outside the lock
    }

    // Net-new for Feature_Servicing_MIDI2LoopbackHardening. For a pair whose creation failed.
    void DiscardPendingRegistrations(_In_ std::wstring const& associationId)
    {
        std::vector<PendingRegistration> removed{};

        {
            std::lock_guard<std::mutex> lock{ m_devicesLock };

            auto cleanId = internal::ToLowerTrimmedWStringCopy(associationId);

            for (auto entry = m_pendingRegistrations.begin(); entry != m_pendingRegistrations.end();)
            {
                if (entry->AssociationId == cleanId)
                {
                    removed.push_back(std::move(*entry));
                    entry = m_pendingRegistrations.erase(entry);
                }
                else
                {
                    ++entry;
                }
            }
        }

        // released here, outside the lock
    }

    // Net-new for Feature_Servicing_MIDI2LoopbackHardening. Replaces SetDevice. Registrations which
    // were waiting for this pair go into it, in the order they arrived, before anyone else can see
    // it, so a later open still replaces an earlier one exactly as it would have.
    void SetDeviceApplyingPendingRegistrations(_In_ std::wstring const& associationId, _In_ MidiLoopbackDevice const& device)
    {
        auto newDevice = std::make_shared<MidiLoopbackDevice>(device);

        std::vector<PendingRegistration> applied{};
        std::shared_ptr<MidiLoopbackDevice> replaced{};

        {
            std::lock_guard<std::mutex> lock{ m_devicesLock };

            auto cleanId = internal::ToLowerTrimmedWStringCopy(associationId);

            for (auto entry = m_pendingRegistrations.begin(); entry != m_pendingRegistrations.end();)
            {
                if (entry->AssociationId == cleanId)
                {
                    applied.push_back(std::move(*entry));
                    entry = m_pendingRegistrations.erase(entry);
                }
                else
                {
                    ++entry;
                }
            }

            // Nothing else can reach the new device yet, so it needs no lock of its own. Anything
            // these replace is still held by applied, so nothing is destroyed under this lock.
            for (auto const& registration : applied)
            {
                if (registration.IsSideA)
                {
                    newDevice->RegisterEndpointA(registration.Callback);
                }
                else
                {
                    newDevice->RegisterEndpointB(registration.Callback);
                }
            }

            if (auto existing = m_devices.find(cleanId); existing != m_devices.end())
            {
                replaced = existing->second;
            }

            m_devices[cleanId] = newDevice;
        }

        // applied and replaced are released here, outside the lock
    }


private:
    // Net-new for Feature_Servicing_MIDI2LoopbackHardening.
    struct PendingRegistration
    {
        std::wstring AssociationId{};
        bool IsSideA{ false };
        wil::com_ptr_nothrow<IMidiCallback> Callback{};
    };

    std::vector<PendingRegistration> m_pendingRegistrations{};
};
