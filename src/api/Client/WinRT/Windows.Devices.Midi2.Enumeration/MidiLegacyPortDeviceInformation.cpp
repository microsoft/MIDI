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


    void AddSingleDeviceInformationEntriesToPortsList(
        _In_ enumeration::DeviceInformation const& device,
        _Inout_ collections::IVector<legacy::MidiLegacyPortDeviceInformation>& ports)
    {
        auto midiLegacyPortDeviceInformation = winrt::make_self<MidiLegacyPortDeviceInformation>();

        if (midiLegacyPortDeviceInformation != nullptr)
        {
            midiLegacyPortDeviceInformation->InternalInitialize(device.Name(), device.Id(), device.Properties());
            ports.Append(*midiLegacyPortDeviceInformation);
        }
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
                midiLegacyPortDeviceInformation->InternalInitialize(device.Name(), device.Id(), device.Properties());
                ports.Append(*midiLegacyPortDeviceInformation);
            }
        }
    }

    void AddAllPnpObjectEntriesToPortsList(
        _In_ pnp::PnpObjectCollection const& devices,
        _Inout_ collections::IVector<legacy::MidiLegacyPortDeviceInformation>& ports)
    {
        for (auto const& device : devices)
        {
            auto midiLegacyPortDeviceInformation = winrt::make_self<MidiLegacyPortDeviceInformation>();

            if (midiLegacyPortDeviceInformation != nullptr)
            {
                // TODO

                midiLegacyPortDeviceInformation->InternalInitialize(L"", device.Id(), device.Properties());
                ports.Append(*midiLegacyPortDeviceInformation);
            }
        }
    }




    _Use_decl_annotations_
    legacy::MidiLegacyPortDeviceInformation MidiLegacyPortDeviceInformation::CreateFromPortDeviceId(
        winrt::hstring const& portDeviceId) noexcept
    {
        try
        {
            // STA-safe: the inner co_await runs on the thread pool.
            // TODO: future migration: rebuild this entirely on MidiPnpDeviceInfo
            // once we have an AQS-name -> DEVPKEY property-bag synthesizer so
            // we can drop the DeviceInformation dependency for the public
            // Properties() accessor.
            auto device = internal::RunOnBackgroundThreadAndWait(
                [portDeviceId]()
                {
                    return enumeration::DeviceInformation::CreateFromIdAsync(
                        portDeviceId,
                        GetAdditionalPropertiesList(),
                        enumeration::DeviceInformationKind::DeviceInterface);
                });

            if (device == nullptr) return nullptr;

            auto midiLegacyPortDeviceInformation = winrt::make_self<MidiLegacyPortDeviceInformation>();
            if (midiLegacyPortDeviceInformation == nullptr) return nullptr;

            midiLegacyPortDeviceInformation->InternalInitialize(device.Name(), device.Id(), device.Properties());

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
    winrt::hstring MidiLegacyPortDeviceInformation::InternalGetSelectorForSourcePortsForContainer(winrt::guid const& containerId) noexcept
    {
        winrt::hstring sourceClass = internal::GuidToString(MidiLegacyPortDeviceInformation::Midi1SourcePortInterfaceClass()).c_str();

        winrt::hstring selector =
            L"System.Devices.InterfaceClassGuid:=\"" + sourceClass + L"\"  AND " +
            L"System.Devices.ContainerId:=\"" + internal::GuidToString(containerId) + L"\" AND " +
            L"System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";

        return selector;
    }

    _Use_decl_annotations_
    winrt::hstring MidiLegacyPortDeviceInformation::InternalGetSelectorForDestinationPortsForContainer(winrt::guid const& containerId) noexcept
    {
        winrt::hstring destinationClass = internal::GuidToString(MidiLegacyPortDeviceInformation::Midi1DestinationPortInterfaceClass()).c_str();

        winrt::hstring selector =
            L"System.Devices.InterfaceClassGuid:=\"" + destinationClass + L"\" AND " +
            L"System.Devices.ContainerId:=\"" + internal::GuidToString(containerId) + L"\" AND " +
            L"System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";

        return selector;
    }

    //_Use_decl_annotations_
    //winrt::hstring MidiLegacyPortDeviceInformation::InternalGetSelectorForSourceAndDestinationPortsForContainer(winrt::guid const& containerId) noexcept
    //{
    //    winrt::hstring sourceClass = internal::GuidToString(MidiLegacyPortDeviceInformation::Midi1SourcePortInterfaceClass()).c_str();
    //    winrt::hstring destinationClass = internal::GuidToString(MidiLegacyPortDeviceInformation::Midi1DestinationPortInterfaceClass()).c_str();


    //    winrt::hstring selector =
    //        L"(System.Devices.InterfaceClassGuid:=\"" + sourceClass + L"\" OR " +
    //        L"System.Devices.InterfaceClassGuid:=\"" + destinationClass + L"\") AND " +
    //        L"System.Devices.ContainerId:=\"" + internal::GuidToString(containerId) + L"\" AND " +
    //        L"System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";

    //    return selector;
    //}


    // used by the watcher
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

    winrt::hstring MidiLegacyPortDeviceInformation::InternalGetSelectorForSourcePorts() noexcept
    {
        winrt::hstring sourceClass = internal::GuidToString(MidiLegacyPortDeviceInformation::Midi1SourcePortInterfaceClass()).c_str();
        winrt::hstring destinationClass = internal::GuidToString(MidiLegacyPortDeviceInformation::Midi1DestinationPortInterfaceClass()).c_str();


        winrt::hstring selector =
            L"System.Devices.InterfaceClassGuid:=\"" + sourceClass + L"\" AND " +
            L"System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";

        return selector;
    }

    winrt::hstring MidiLegacyPortDeviceInformation::InternalGetSelectorForDestinationPorts() noexcept
    {
        winrt::hstring sourceClass = internal::GuidToString(MidiLegacyPortDeviceInformation::Midi1SourcePortInterfaceClass()).c_str();
        winrt::hstring destinationClass = internal::GuidToString(MidiLegacyPortDeviceInformation::Midi1DestinationPortInterfaceClass()).c_str();


        winrt::hstring selector =
            L"System.Devices.InterfaceClassGuid:=\"" + destinationClass + L"\" AND " +
            L"System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";

        return selector;
    }


    //_Use_decl_annotations_
    //winrt::hstring MidiLegacyPortDeviceInformation::InternalGetSelectorForSourceAndDestinationPortsForParentDeviceInstanceId(winrt::hstring const& parentDeviceInstanceId) noexcept
    //{
    //    winrt::hstring sourceClass = internal::GuidToString(MidiLegacyPortDeviceInformation::Midi1SourcePortInterfaceClass()).c_str();
    //    winrt::hstring destinationClass = internal::GuidToString(MidiLegacyPortDeviceInformation::Midi1DestinationPortInterfaceClass()).c_str();


    //    winrt::hstring selector =
    //        L"(System.Devices.InterfaceClassGuid:=\"" + sourceClass + L"\" OR " +
    //        L"System.Devices.InterfaceClassGuid:=\"" + destinationClass + L"\") AND " +
    //        L"System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True AND " +
    //        L"System.Devices.Parent:=\"" + parentDeviceInstanceId + L"\"";

    //    return selector;
    //}

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
            // turns out, having an OR in the AQS query is much slower than just making two passes
            // I was seeing 147 ms for the OR query, vs 43ms for two separate queries + merge
            
            for (auto const& selector: { InternalGetSelectorForSourcePorts(), InternalGetSelectorForDestinationPorts() })
            {
                // STA-safe enumeration via background-thread continuation.
                auto devices = internal::RunOnBackgroundThreadAndWait(
                    [selector]()
                    {
                        return enumeration::DeviceInformation::FindAllAsync(
                            selector,
                            GetAdditionalPropertiesList(),
                            enumeration::DeviceInformationKind::DeviceInterface);
                    });

                for (auto const& device : devices)
                {
                    auto midiLegacyPortDeviceInformation = winrt::make_self<MidiLegacyPortDeviceInformation>();

                    if (midiLegacyPortDeviceInformation != nullptr)
                    {
                        midiLegacyPortDeviceInformation->InternalInitialize(device.Name(), device.Id(), device.Properties());
                        results.Append(*midiLegacyPortDeviceInformation);
                    }
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

        auto sourceSelector = winrt::Windows::Devices::Midi::MidiInPort::GetDeviceSelector() + L" AND System.ItemNameDisplay:= \"" + portName + L"\"";
        auto destinationSelector = winrt::Windows::Devices::Midi::MidiOutPort::GetDeviceSelector() + L" AND System.ItemNameDisplay:= \"" + portName + L"\"";

        try
        {
            for (auto const& selector : { sourceSelector , destinationSelector })
            {
                // get all sources
                auto devices = enumeration::DeviceInformation::FindAllAsync(
                    selector,
                    GetAdditionalPropertiesList(),
                    enumeration::DeviceInformationKind::DeviceInterface).get();

                AddAllDeviceInformationEntriesToPortsList(devices, results);

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

        auto sourceSelector = winrt::Windows::Devices::Midi::MidiInPort::GetDeviceSelector() + L" AND System.Devices.ContainerId := \"" + internal::GuidToString(containerId) + L"\"";
        auto destinationSelector = winrt::Windows::Devices::Midi::MidiOutPort::GetDeviceSelector() + L" AND System.Devices.ContainerId := \"" + internal::GuidToString(containerId) + L"\"";

        try
        {
            for (auto const& selector : { sourceSelector , destinationSelector })
            {
                // get all sources
                auto devices = enumeration::DeviceInformation::FindAllAsync(
                    selector,
                    GetAdditionalPropertiesList(),
                    enumeration::DeviceInformationKind::DeviceInterface).get();

                AddAllDeviceInformationEntriesToPortsList(devices, results);

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

    //_Use_decl_annotations_
    //collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> MidiLegacyPortDeviceInformation::FindAllForParentDevice(
    //    winrt::hstring const& parentDeviceInstanceId) noexcept
    //{
    //    auto results = winrt::single_threaded_vector<legacy::MidiLegacyPortDeviceInformation>();

    //    auto sourceSelector = winrt::Windows::Devices::Midi::MidiInPort::GetDeviceSelector() + L" AND System.Devices.Parent:= \"" + parentDeviceInstanceId + L"\"";
    //    auto destinationSelector = winrt::Windows::Devices::Midi::MidiOutPort::GetDeviceSelector() + L" AND System.Devices.Parent:= \"" + parentDeviceInstanceId + L"\"";

    //    try
    //    {
    //        for (auto const& selector : { sourceSelector , destinationSelector })
    //        {
    //            //// get all sources
    //            //auto devices = enumeration::DeviceInformation::FindAllAsync(
    //            //    selector,
    //            //    GetAdditionalPropertiesList(),
    //            //    enumeration::DeviceInformationKind::DeviceInterface).get();

    //            // get all sources
    //            auto devices = pnp::PnpObject::FindAllAsync(
    //                pnp::PnpObjectType::DeviceInterface,
    //                GetAdditionalPropertiesList(),
    //                selector
    //                ).get();

    //            AddAllPnpObjectEntriesToPortsList(devices, results);

    //        }
    //    }
    //    catch (winrt::hresult_error const& ex)
    //    {
    //        LOG_IF_FAILED(ex.code());   // this also generates a fallback error with file and line number info

    //        TraceLoggingWrite(
    //            Midi2SdkTelemetryProvider::Provider(),
    //            MIDI_SDK_TRACE_EVENT_ERROR,
    //            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
    //            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
    //            TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
    //            TraceLoggingWideString(L"Error finding all MIDI 1 ports for UMP endpoint", MIDI_SDK_TRACE_MESSAGE_FIELD),
    //            TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
    //            TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
    //        );

    //    }
    //    catch (...)
    //    {
    //        LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

    //        TraceLoggingWrite(
    //            Midi2SdkTelemetryProvider::Provider(),
    //            MIDI_SDK_TRACE_EVENT_ERROR,
    //            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
    //            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
    //            TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
    //            TraceLoggingWideString(L"Error finding all MIDI 1 ports for UMP endpoint", MIDI_SDK_TRACE_MESSAGE_FIELD)
    //        );

    //    }

    //    return results.GetView();
    //}

    //collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> MidiLegacyPortDeviceInformation::FindAllForParentDevice(
    //    _In_ winrt::hstring const& parentDeviceInstanceId,
    //    _In_ midi2enum::Midi1PortFlow const& flow) noexcept
    //{
    //    winrt::hstring selector;

    //    if (flow == midi2enum::Midi1PortFlow::MidiMessageSource)
    //    {
    //        selector = InternalGetSelectorForSourcePortsForParentDeviceInstanceId(parentDeviceInstanceId);
    //    }
    //    else
    //    {
    //        selector = InternalGetSelectorForDestinationPortsForParentDeviceInstanceId(parentDeviceInstanceId);
    //    }
    //

    //    auto results = winrt::single_threaded_vector<legacy::MidiLegacyPortDeviceInformation>();

    //    try
    //    {
    //        auto devices = enumeration::DeviceInformation::FindAllAsync(
    //            selector,
    //            GetAdditionalPropertiesList(),
    //            enumeration::DeviceInformationKind::DeviceInterface).get();

    //        AddAllDeviceInformationEntriesToPortsList(devices, results);


    //    }
    //    catch (winrt::hresult_error const& ex)
    //    {
    //        LOG_IF_FAILED(ex.code());   // this also generates a fallback error with file and line number info

    //        TraceLoggingWrite(
    //            Midi2SdkTelemetryProvider::Provider(),
    //            MIDI_SDK_TRACE_EVENT_ERROR,
    //            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
    //            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
    //            TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
    //            TraceLoggingWideString(L"Error finding MIDI 1 ports for UMP endpoint", MIDI_SDK_TRACE_MESSAGE_FIELD),
    //            TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
    //            TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
    //        );

    //    }
    //    catch (...)
    //    {
    //        LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

    //        TraceLoggingWrite(
    //            Midi2SdkTelemetryProvider::Provider(),
    //            MIDI_SDK_TRACE_EVENT_ERROR,
    //            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
    //            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
    //            TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
    //            TraceLoggingWideString(L"Error finding MIDI 1 ports for UMP endpoint", MIDI_SDK_TRACE_MESSAGE_FIELD)
    //        );

    //    }

    //    return results.GetView();
    //}



    midi2enum::MidiParentDeviceInformation MidiLegacyPortDeviceInformation::GetParentDeviceInformation() const noexcept
    {
        return midi2enum::MidiParentDeviceInformation::CreateFromId(m_parentDeviceInstanceId);
    }



    _Use_decl_annotations_
    collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> MidiLegacyPortDeviceInformation::FindAllForEndpoint(
         winrt::hstring const& endpointDeviceId) noexcept
    {
        auto results = winrt::single_threaded_vector<legacy::MidiLegacyPortDeviceInformation>();

        try
        {
            // Resolve the endpoint's container id via cfgmgr -- synchronous,
            // no PnP service round-trip, STA-safe.
            auto endpoint = internal::MidiPnpDeviceInfo::CreateFromInterfaceId(
                std::wstring_view{ endpointDeviceId });

            if (!endpoint.has_value()) return results.GetView();

            auto containerIdOpt = endpoint->ContainerId();
            if (!containerIdOpt.has_value()) return results.GetView();

            auto containerId = containerIdOpt.value();

            for (auto const& selector : {
                InternalGetSelectorForSourcePortsForContainer(containerId),
                InternalGetSelectorForDestinationPortsForContainer(containerId) })
            {
                auto devices = internal::RunOnBackgroundThreadAndWait(
                    [selector]()
                    {
                        return enumeration::DeviceInformation::FindAllAsync(
                            selector,
                            GetAdditionalPropertiesList(),
                            enumeration::DeviceInformationKind::DeviceInterface);
                    });

                for (auto const& device : devices)
                {
                    // Use TryLookup so a single missing AssociatedUMP key
                    // doesn't drop the rest of the results.
                    auto raw = device.Properties().TryLookup(STRING_PKEY_MIDI_AssociatedUMP);
                    if (raw == nullptr) continue;

                    auto id = winrt::unbox_value_or<winrt::hstring>(raw, L"");
                    if (id.empty()) continue;

                    if (internal::NormalizeEndpointInterfaceIdHStringCopy(id) == endpointDeviceId)
                    {
                        AddSingleDeviceInformationEntriesToPortsList(device, results);
                    }
                }
            }
        }
        catch (...)
        {
            // can't find endpoint
        }

        return results.GetView();
    }

    _Use_decl_annotations_
    collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> MidiLegacyPortDeviceInformation::FindAllForEndpoint(
        winrt::hstring const& endpointDeviceId,
        midi2enum::Midi1PortFlow const& flow) noexcept
    {
        auto results = winrt::single_threaded_vector<legacy::MidiLegacyPortDeviceInformation>();

        try
        {
            auto endpoint = internal::MidiPnpDeviceInfo::CreateFromInterfaceId(
                std::wstring_view{ endpointDeviceId });

            if (!endpoint.has_value()) return results.GetView();

            auto containerIdOpt = endpoint->ContainerId();
            if (!containerIdOpt.has_value()) return results.GetView();

            auto containerId = containerIdOpt.value();

            auto selector = flow == midi2enum::Midi1PortFlow::MidiMessageSource ?
                InternalGetSelectorForSourcePortsForContainer(containerId) :
                InternalGetSelectorForDestinationPortsForContainer(containerId);

            auto devices = internal::RunOnBackgroundThreadAndWait(
                [selector]()
                {
                    return enumeration::DeviceInformation::FindAllAsync(
                        selector,
                        GetAdditionalPropertiesList(),
                        enumeration::DeviceInformationKind::DeviceInterface);
                });

            for (auto const& device : devices)
            {
                auto raw = device.Properties().TryLookup(STRING_PKEY_MIDI_AssociatedUMP);
                if (raw == nullptr) continue;

                auto id = winrt::unbox_value_or<winrt::hstring>(raw, L"");
                if (id.empty()) continue;

                if (internal::NormalizeEndpointInterfaceIdHStringCopy(id) == endpointDeviceId)
                {
                    AddSingleDeviceInformationEntriesToPortsList(device, results);
                }
            }
        }
        catch (...)
        {
            // can't find endpoint
        }

        return results.GetView();

    }







    _Use_decl_annotations_
    bool MidiLegacyPortDeviceInformation::InternalInitialize(
        winrt::hstring name, 
        winrt::hstring id, 
        collections::IMapView<winrt::hstring, IInspectable> const& properties) noexcept
    {
     //   if (deviceInformation == nullptr) return false;

        for (auto&& [key, value] : properties)
        {
            // insert does a replace if the key exists in the map
            m_properties.Insert(key, value);
        }

        if (properties.HasKey(L"System.Devices.InterfaceClassGuid"))
        {
            auto interfaceClassGuid = internal::GetDeviceInfoProperty<winrt::guid>(properties, L"System.Devices.InterfaceClassGuid", winrt::guid());

            if (interfaceClassGuid == MidiLegacyPortDeviceInformation::Midi1SourcePortInterfaceClass())
            {
                m_portFlow = midi2enum::Midi1PortFlow::MidiMessageSource;
            }
            else if (interfaceClassGuid == MidiLegacyPortDeviceInformation::Midi1DestinationPortInterfaceClass())
            {
                m_portFlow = midi2enum::Midi1PortFlow::MidiMessageDestination;
            }
        }

        if (properties.HasKey(STRING_PKEY_MIDI_ServiceAssignedPortNumber))
        {
            m_portNumber = internal::GetDeviceInfoProperty<uint32_t>(properties, STRING_PKEY_MIDI_ServiceAssignedPortNumber, 0);
        }

        m_name = name;
        m_id = internal::NormalizeEndpointInterfaceIdHStringCopy(id);


        winrt::hstring deviceInstanceId{};
        if (properties.HasKey(L"System.Devices.DeviceInstanceId"))
        {
            deviceInstanceId = internal::GetDeviceInfoProperty<winrt::hstring>(properties, L"System.Devices.DeviceInstanceId", winrt::hstring());
        }

        // Resolve the parent device instance id via MidiPnpDeviceInfo
        // (cfgmgr-backed, synchronous, STA-safe).
        m_parentDeviceInstanceId = {};

        if (!deviceInstanceId.empty())
        {
            if (auto pnpInfo = internal::MidiPnpDeviceInfo::CreateFromInstanceId(
                    std::wstring_view{ deviceInstanceId }))
            {
                m_parentDeviceInstanceId = internal::NormalizeDeviceInstanceIdHStringCopy(pnpInfo->ParentInstanceId());
            }
            else
            {
                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_WARNING,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"MidiPnpDeviceInfo::CreateFromInstanceId returned no value", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(deviceInstanceId.c_str(), "device instance id")
                );
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
