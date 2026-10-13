// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Include devpkey.h and MidiDefs.h first. devpkey.h has no include guard, so it is not included here.

#include <cfgmgr32.h>
#include <cstring>
#include <string>
#include <vector>

namespace Midi1PortRegistration
{
    // True only when the port's interface is enabled, has a port number, and already holds every
    // value in properties. A DEVPROP_TYPE_EMPTY entry deletes its property, so it must be absent.
    inline bool IsCurrent(
        _In_ std::wstring const& midi1PortInterfaceId,
        _In_ std::vector<DEVPROPERTY> const& properties) noexcept
    {
        try
        {
            std::vector<BYTE> value{ };
            DEVPROPTYPE type{ DEVPROP_TYPE_EMPTY };

            auto read = [&](DEVPROPKEY const& key) -> CONFIGRET
                {
                    ULONG size{ 0 };
                    type = DEVPROP_TYPE_EMPTY;
                    value.clear();

                    auto result = CM_Get_Device_Interface_PropertyW(midi1PortInterfaceId.c_str(), &key, &type, nullptr, &size, 0);

                    if (result == CR_BUFFER_SMALL)
                    {
                        value.resize(size);
                        result = CM_Get_Device_Interface_PropertyW(midi1PortInterfaceId.c_str(), &key, &type, value.data(), &size, 0);
                        value.resize(size);
                    }

                    return result;
                };

            DEVPROP_BOOLEAN enabled{ DEVPROP_FALSE };

            if (read(DEVPKEY_DeviceInterface_Enabled) != CR_SUCCESS || type != DEVPROP_TYPE_BOOLEAN || value.size() != sizeof(enabled))
            {
                return false;
            }

            memcpy(&enabled, value.data(), sizeof(enabled));

            if (enabled != DEVPROP_TRUE)
            {
                return false;
            }

            UINT32 portNumber{ 0 };

            if (read(PKEY_MIDI_ServiceAssignedPortNumber) != CR_SUCCESS || type != DEVPROP_TYPE_UINT32 || value.size() != sizeof(portNumber))
            {
                return false;
            }

            memcpy(&portNumber, value.data(), sizeof(portNumber));

            if (portNumber == 0)
            {
                return false;
            }

            for (auto const& property : properties)
            {
                auto const result = read(property.CompKey.Key);

                if (property.Type == DEVPROP_TYPE_EMPTY)
                {
                    if (result != CR_NO_SUCH_VALUE)
                    {
                        return false;
                    }

                    continue;
                }

                if (result != CR_SUCCESS || type != property.Type || value.size() != property.BufferSize)
                {
                    return false;
                }

                if (property.BufferSize > 0 && memcmp(value.data(), property.Buffer, property.BufferSize) != 0)
                {
                    return false;
                }
            }

            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}
