// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiLegacyPortDeviceInformation.h"
#include "Legacy.MidiLegacyPortDeviceInformation.g.cpp"


namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy::implementation
{
    _Use_decl_annotations_
    legacy::MidiLegacyPortDeviceInformation MidiLegacyPortDeviceInformation::CreateFromPortDeviceId(
        winrt::hstring const& portDeviceId) noexcept
    {
        try
        {
            auto device = enumeration::DeviceInformation::CreateFromIdAsync(
                portDeviceId, 
                GetAdditionalPropertiesList(),
                enumeration::DeviceInformationKind::DeviceInterface).get();

            if (device == nullptr) return nullptr;

            auto midiLegacyPortDeviceInformation = winrt::make_self<MidiLegacyPortDeviceInformation>();
            if (midiLegacyPortDeviceInformation == nullptr) return nullptr;

            // TODO: Ensure that the return device matches the flow
            midiLegacyPortDeviceInformation->InternalInitialize(device);

            return *midiLegacyPortDeviceInformation;
        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(ex.code());   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error creating legacy MIDI port device information", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            return nullptr;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error creating legacy MIDI port device information", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return nullptr;
        }
    }

    _Use_decl_annotations_
    winrt::hstring MidiLegacyPortDeviceInformation::InternalGetSelectorForSourceAndDestinationPortsForContainer(winrt::guid const& containerId) noexcept
    {
        winrt::hstring sourceClass = internal::GuidToString(MidiLegacyPortDeviceInformation::Midi1SourcePortInterfaceClass()).c_str();
        winrt::hstring destinationClass = internal::GuidToString(MidiLegacyPortDeviceInformation::Midi1DestinationPortInterfaceClass()).c_str();


        winrt::hstring selector =
            L"(System.Devices.InterfaceClassGuid:=\"" + sourceClass + L"\" OR " +
            L"System.Devices.InterfaceClassGuid:=\"" + destinationClass + L"\") AND " +
            L"System.Devices.ContainerId:=\"" + internal::GuidToString(containerId) + L"\" AND " +
            L"System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";

        return selector;
    }

    winrt::hstring MidiLegacyPortDeviceInformation::InternalGetSelectorForSourceAndDestinationPorts() noexcept
    {
        winrt::hstring sourceClass = internal::GuidToString(MidiLegacyPortDeviceInformation::Midi1SourcePortInterfaceClass()).c_str();
        winrt::hstring destinationClass = internal::GuidToString(MidiLegacyPortDeviceInformation::Midi1DestinationPortInterfaceClass()).c_str();


        winrt::hstring selector =
            L"(System.Devices.InterfaceClassGuid:=\"" + sourceClass + L"\" OR " +
            L"System.Devices.InterfaceClassGuid:=\"" + destinationClass + L"\") AND " +
            L"System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";

        return selector;
    }

    _Use_decl_annotations_
    winrt::hstring MidiLegacyPortDeviceInformation::InternalGetSelectorForSourceAndDestinationPortsForParentDeviceInstanceId(winrt::hstring const& parentDeviceInstanceId) noexcept
    {
        winrt::hstring sourceClass = internal::GuidToString(MidiLegacyPortDeviceInformation::Midi1SourcePortInterfaceClass()).c_str();
        winrt::hstring destinationClass = internal::GuidToString(MidiLegacyPortDeviceInformation::Midi1DestinationPortInterfaceClass()).c_str();


        winrt::hstring selector =
            L"(System.Devices.InterfaceClassGuid:=\"" + sourceClass + L"\" OR " +
            L"System.Devices.InterfaceClassGuid:=\"" + destinationClass + L"\") AND " +
            L"System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True AND " +
            L"System.Devices.Parent:=\"" + parentDeviceInstanceId + L"\"";

        return selector;
    }

    _Use_decl_annotations_
    winrt::hstring MidiLegacyPortDeviceInformation::InternalGetSelectorForSourcePortsForParentDeviceInstanceId(winrt::hstring const& parentDeviceInstanceId) noexcept
    {
        winrt::hstring sourceClass = internal::GuidToString(MidiLegacyPortDeviceInformation::Midi1SourcePortInterfaceClass()).c_str();

        winrt::hstring selector =
            L"System.Devices.InterfaceClassGuid:=\"" + sourceClass + L"\" AND " +
            L"System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True AND " +
            L"System.Devices.Parent:=\"" + parentDeviceInstanceId + L"\"";

        return selector;
    }

    _Use_decl_annotations_
    winrt::hstring MidiLegacyPortDeviceInformation::InternalGetSelectorForDestinationPortsForParentDeviceInstanceId(winrt::hstring const& parentDeviceInstanceId) noexcept
    {
        winrt::hstring destinationClass = internal::GuidToString(MidiLegacyPortDeviceInformation::Midi1DestinationPortInterfaceClass()).c_str();

        winrt::hstring selector =
            L"System.Devices.InterfaceClassGuid:=\"" + destinationClass + L"\" AND " +
            L"System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True AND " +
            L"System.Devices.Parent:=\"" + parentDeviceInstanceId + L"\"";

        return selector;
    }


    collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> MidiLegacyPortDeviceInformation::FindAll() noexcept
    {
        auto results = winrt::single_threaded_vector<legacy::MidiLegacyPortDeviceInformation>();

        try
        {
            // get all sources
            auto devices = enumeration::DeviceInformation::FindAllAsync(
                InternalGetSelectorForSourceAndDestinationPorts(),
                GetAdditionalPropertiesList(),
                enumeration::DeviceInformationKind::DeviceInterface).get();

            for (auto const& device: devices)
            {
                auto midiLegacyPortDeviceInformation = winrt::make_self<MidiLegacyPortDeviceInformation>();

                if (midiLegacyPortDeviceInformation != nullptr)
                {
                    midiLegacyPortDeviceInformation->InternalInitialize(device);
                    results.Append(*midiLegacyPortDeviceInformation);
                }
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(ex.code());   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error enumerating legacy ports", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error enumerating legacy ports", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

        }

        return results.GetView();
    }


    void AddAllDeviceInformationEntriesToPortsList(
        _In_ enumeration::DeviceInformationCollection const& devices, 
        _Inout_ collections::IVector<legacy::MidiLegacyPortDeviceInformation>& ports)
    {
        for (auto const& device : devices)
        {
            auto midiLegacyPortDeviceInformation = winrt::make_self<MidiLegacyPortDeviceInformation>();

            if (midiLegacyPortDeviceInformation != nullptr)
            {
                midiLegacyPortDeviceInformation->InternalInitialize(device);
                ports.Append(*midiLegacyPortDeviceInformation);
            }
        }
    }



    _Use_decl_annotations_
    collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> MidiLegacyPortDeviceInformation::FindAll(
        midi2enum::Midi1PortFlow const& flow) noexcept
    {
        auto results = winrt::single_threaded_vector<legacy::MidiLegacyPortDeviceInformation>();

        try
        {

            if (flow == midi2enum::Midi1PortFlow::MidiMessageSource)
            {
                // get all sources
                auto sourceDevices = enumeration::DeviceInformation::FindAllAsync(
                    winrt::Windows::Devices::Midi::MidiInPort::GetDeviceSelector(),
                    GetAdditionalPropertiesList()).get();

                AddAllDeviceInformationEntriesToPortsList(sourceDevices, results);

            }
            else if (flow == midi2enum::Midi1PortFlow::MidiMessageDestination)
            {
                // get all destinations
                auto destinationDevices = enumeration::DeviceInformation::FindAllAsync(
                    winrt::Windows::Devices::Midi::MidiOutPort::GetDeviceSelector(),
                    GetAdditionalPropertiesList(),
                    enumeration::DeviceInformationKind::DeviceInterface).get();

                AddAllDeviceInformationEntriesToPortsList(destinationDevices, results);
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(ex.code());   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error creating legacy MIDI port device information", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error creating legacy MIDI port device information", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

        }

        return results.GetView();
    }


    _Use_decl_annotations_
    collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> MidiLegacyPortDeviceInformation::FindAllForName(
        _In_ winrt::hstring const& portName) noexcept
    {
        auto results = winrt::single_threaded_vector<legacy::MidiLegacyPortDeviceInformation>();

        try
        {
            winrt::hstring selector = InternalGetSelectorForSourceAndDestinationPorts() +
                L" AND ItemNameDisplay:=\"" + portName + L"\"";

            auto devices = enumeration::DeviceInformation::FindAllAsync(
                selector,
                GetAdditionalPropertiesList(),
                enumeration::DeviceInformationKind::DeviceInterface).get();

            AddAllDeviceInformationEntriesToPortsList(devices, results);


        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(ex.code());   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error finding all MIDI 1 ports for port name", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error finding all MIDI 1 ports for port name", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

        }

        return results.GetView();



    }

    _Use_decl_annotations_
    collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> MidiLegacyPortDeviceInformation::FindAllForContainer(
        winrt::guid const& containerId) noexcept
    {
        auto results = winrt::single_threaded_vector<legacy::MidiLegacyPortDeviceInformation>();

        try
        {
            auto devices = enumeration::DeviceInformation::FindAllAsync(
                InternalGetSelectorForSourceAndDestinationPortsForContainer(containerId),
                GetAdditionalPropertiesList(),
                enumeration::DeviceInformationKind::DeviceInterface).get();

            AddAllDeviceInformationEntriesToPortsList(devices, results);


        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(ex.code());   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error finding all MIDI 1 ports for container", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error finding all MIDI 1 ports for container", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

        }

        return results.GetView();

    }

    _Use_decl_annotations_
    collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> MidiLegacyPortDeviceInformation::FindAllForParentDevice(
        winrt::hstring const& parentDeviceInstanceId) noexcept
    {
        auto results = winrt::single_threaded_vector<legacy::MidiLegacyPortDeviceInformation>();

        try
        {
            auto devices = enumeration::DeviceInformation::FindAllAsync(
                InternalGetSelectorForSourceAndDestinationPortsForParentDeviceInstanceId(parentDeviceInstanceId),
                GetAdditionalPropertiesList(),
                enumeration::DeviceInformationKind::DeviceInterface).get();

            AddAllDeviceInformationEntriesToPortsList(devices, results);


        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(ex.code());   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error finding all MIDI 1 ports for UMP endpoint", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error finding all MIDI 1 ports for UMP endpoint", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

        }

        return results.GetView();
    }

    collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> MidiLegacyPortDeviceInformation::FindAllForParentDevice(
        _In_ winrt::hstring const& parentDeviceInstanceId,
        _In_ midi2enum::Midi1PortFlow const& flow) noexcept
    {
        winrt::hstring selector;

        if (flow == midi2enum::Midi1PortFlow::MidiMessageSource)
        {
            selector = InternalGetSelectorForSourcePortsForParentDeviceInstanceId(parentDeviceInstanceId);
        }
        else
        {
            selector = InternalGetSelectorForDestinationPortsForParentDeviceInstanceId(parentDeviceInstanceId);
        }
    

        auto results = winrt::single_threaded_vector<legacy::MidiLegacyPortDeviceInformation>();

        try
        {
            auto devices = enumeration::DeviceInformation::FindAllAsync(
                selector,
                GetAdditionalPropertiesList(),
                enumeration::DeviceInformationKind::DeviceInterface).get();

            AddAllDeviceInformationEntriesToPortsList(devices, results);


        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(ex.code());   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error finding MIDI 1 ports for UMP endpoint", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error finding MIDI 1 ports for UMP endpoint", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

        }

        return results.GetView();
    }

    _Use_decl_annotations_
    bool MidiLegacyPortDeviceInformation::InternalInitialize(enumeration::DeviceInformation const& deviceInformation) noexcept
    {
        if (deviceInformation == nullptr) return false;

        for (auto&& [key, value] : deviceInformation.Properties())
        {
            // insert does a replace if the key exists in the map
            m_properties.Insert(key, value);
        }

        if (deviceInformation.Properties().HasKey(L"System.Devices.InterfaceClassGuid"))
        {
            auto interfaceClassGuid = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::guid>(L"System.Devices.InterfaceClassGuid", deviceInformation, winrt::guid());

            if (interfaceClassGuid == MidiLegacyPortDeviceInformation::Midi1SourcePortInterfaceClass())
            {
                m_portFlow = midi2enum::Midi1PortFlow::MidiMessageSource;
            }
            else if (interfaceClassGuid == MidiLegacyPortDeviceInformation::Midi1DestinationPortInterfaceClass())
            {
                m_portFlow = midi2enum::Midi1PortFlow::MidiMessageDestination;
            }
        }

        if (deviceInformation.Properties().HasKey(STRING_PKEY_MIDI_ServiceAssignedPortNumber))
        {
            m_portNumber = internal::SafeGetSwdPropertyFromDeviceInformation<uint32_t>(STRING_PKEY_MIDI_ServiceAssignedPortNumber, deviceInformation, 0);
        }

        m_name = deviceInformation.Name();
        m_id = internal::NormalizeEndpointInterfaceIdHStringCopy(deviceInformation.Id());


        winrt::hstring deviceInstanceId{};
        if (deviceInformation.Properties().HasKey(L"System.Devices.DeviceInstanceId"))
        {
            deviceInstanceId = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.DeviceInstanceId", deviceInformation, winrt::hstring());
        }

        // get the parent
        if (!deviceInstanceId.empty())
        {
            auto pnpObject = pnp::PnpObject::CreateFromIdAsync(
                pnp::PnpObjectType::Device,
                deviceInstanceId,
                { L"System.Devices.Parent" }).get();

            if (pnpObject != nullptr && pnpObject.Properties().HasKey(L"System.Devices.Parent"))
            {
                m_parentDeviceInstanceId = internal::GetDeviceInfoProperty<winrt::hstring>(pnpObject.Properties(), L"System.Devices.Parent", winrt::hstring());
            }

        }

        return true;
    }

    _Use_decl_annotations_
    bool MidiLegacyPortDeviceInformation::InternalUpdateFromDeviceInformationUpdate(
        enumeration::DeviceInformationUpdate const& deviceInformationUpdate) noexcept
    {
        if (deviceInformationUpdate == nullptr) return false;
       
        // most properties cannot change. We're checking just a few of them here
        for (auto&& [key, value] : deviceInformationUpdate.Properties())
        {
            // insert does a replace if the key exists in the map

            m_properties.Insert(key, value);

            if (key == L"System.Devices.FriendlyName" || key == L"System.ItemNameDisplay")
            {
                auto newName = winrt::unbox_value<winrt::hstring>(value);

                if (!newName.empty())
                {
                    m_name = newName;
                }
            }
            else if (key == STRING_PKEY_MIDI_ServiceAssignedPortNumber)
            {
                m_portNumber = winrt::unbox_value<uint32_t>(value);
            }
            else
            {
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Unexpected changed property", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(key.c_str(), "property key")
                );
            }

        }

        return true;
    }


    collections::IVectorView<hstring> MidiLegacyPortDeviceInformation::GetAdditionalPropertiesList() noexcept
    {
        auto props = winrt::single_threaded_vector<winrt::hstring>();

        props.Append(L"System.ItemNameDisplay");
        props.Append(L"System.Devices.FriendlyName");

        props.Append(L"System.Devices.ContainerId");
        props.Append(L"System.Devices.Parent");
        props.Append(L"System.Devices.InterfaceClassGuid");

        //props.Append(STRING_DEVPKEY_Device_Parent);
        props.Append(STRING_DEVPKEY_Device_LastKnownParent);
        props.Append(STRING_PKEY_MIDI_DriverDeviceInterface);
        props.Append(STRING_PKEY_MIDI_PortAssignedGroupIndex);
        props.Append(STRING_PKEY_MIDI_NativeDataFormat);
        props.Append(STRING_PKEY_MIDI_TransportLayer);
        props.Append(STRING_PKEY_MIDI_AssociatedUMP);
        props.Append(STRING_PKEY_MIDI_ServiceAssignedPortNumber);

        return props.GetView();
    }

    winrt::hstring MidiLegacyPortDeviceInformation::ToString() const noexcept
    {
        // TODO: Get from resources
        return L"MIDI 1 Port: " + Name() + L": " + Group().ToString();
    }
}
