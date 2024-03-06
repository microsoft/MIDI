// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "stdafx.h"




_Use_decl_annotations_
HRESULT
CMidiDeviceManager::Initialize(
    std::shared_ptr<CMidiPerformanceManager>& PerformanceManager, 
    std::shared_ptr<CMidiEndpointProtocolManager>& EndpointProtocolManager,
    std::shared_ptr<CMidiConfigurationManager>& configurationManager)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    m_ConfigurationManager = configurationManager;


    //wil::com_ptr_nothrow<IMidiServiceConfigurationManagerInterface> serviceConfigManagerInterface;
    //RETURN_IF_FAILED(configurationManager->QueryInterface(__uuidof(IMidiServiceConfigurationManagerInterface), (void**)&serviceConfigManagerInterface));

    // Get the enabled abstraction layers from the registry

    for (auto const& AbstractionLayer : m_ConfigurationManager->GetEnabledTransportAbstractionLayers())
    {
        wil::com_ptr_nothrow<IMidiAbstraction> midiAbstraction;
        wil::com_ptr_nothrow<IMidiEndpointManager> endpointManager;
        wil::com_ptr_nothrow<IMidiAbstractionConfigurationManager> abstractionConfigurationManager;

        try
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Loading abstraction layer", "message"),
                TraceLoggingGuid(AbstractionLayer, "abstraction layer")
            );

            // provide the initial settings for these transports
            auto transportSettingsJson = m_ConfigurationManager->GetSavedConfigurationForTransportAbstraction(AbstractionLayer);

            // changed these from a return-on-fail to just log, so we don't prevent service startup
            // due to one bad abstraction

            LOG_IF_FAILED(CoCreateInstance(AbstractionLayer, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiAbstraction)));

            if (midiAbstraction != nullptr)
            {
                LOG_IF_FAILED(midiAbstraction->Activate(__uuidof(IMidiEndpointManager), (void**)&endpointManager));

                if (endpointManager != nullptr)
                {
                    wil::com_ptr_nothrow<IMidiEndpointProtocolManagerInterface> protocolManager = EndpointProtocolManager.get();

                    auto initializeResult = endpointManager->Initialize((IUnknown*)this, (IUnknown*)protocolManager.get());

                    if (SUCCEEDED(initializeResult))
                    {
                        m_MidiEndpointManagers.emplace(AbstractionLayer, std::move(endpointManager));
                    }
                    else
                    {
                        TraceLoggingWrite(
                            MidiSrvTelemetryProvider::Provider(),
                            __FUNCTION__,
                            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Transport abstraction initialization failed.", "message"),
                            TraceLoggingGuid(AbstractionLayer, "abstraction layer")
                        );
                    }

                }
                else
                {
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        __FUNCTION__,
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Transport abstraction endpoint manager initialization failed.", "message"),
                        TraceLoggingGuid(AbstractionLayer, "abstraction layer")
                    );

                }

                // we only log. This interface is optional
                auto configHR = midiAbstraction->Activate(__uuidof(IMidiAbstractionConfigurationManager), (void**)&abstractionConfigurationManager);
                if (SUCCEEDED(configHR))
                {
                    if (abstractionConfigurationManager != nullptr)
                    {
                        // init the abstraction's config manager
                        auto initializeResult = abstractionConfigurationManager->Initialize(
                            AbstractionLayer,
                            (IUnknown*)this, 
                            (IUnknown*)m_ConfigurationManager.get());

                        if (FAILED(initializeResult))
                        {
                            TraceLoggingWrite(
                                MidiSrvTelemetryProvider::Provider(),
                                __FUNCTION__,
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingHResult(initializeResult, "hresult"),
                                TraceLoggingWideString(L"Failed to initialize abstraction configuration manager.", "message"),
                                TraceLoggingGuid(AbstractionLayer, "abstraction layer")
                            );
                        }
                        else
                        {
                            // don't std::move this because we sill need it
                            m_MidiAbstractionConfigurationManagers[AbstractionLayer] = abstractionConfigurationManager;

                            if (!transportSettingsJson.empty())
                            {
                                CComBSTR response{};
                                response.Empty();

                                auto updateConfigHR = abstractionConfigurationManager->UpdateConfiguration(transportSettingsJson.c_str(), true, &response);

                                // we don't use the response info here.
                                ::SysFreeString(response);


                                if (FAILED(updateConfigHR))
                                {
                                    if (updateConfigHR == E_NOTIMPL)
                                    {
                                        TraceLoggingWrite(
                                            MidiSrvTelemetryProvider::Provider(),
                                            __FUNCTION__,
                                            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                                            TraceLoggingPointer(this, "this"),
                                            TraceLoggingHResult(updateConfigHR, "hresult"),
                                            TraceLoggingWideString(L"Config update not implemented for this abstraction.", "message"),
                                            TraceLoggingGuid(AbstractionLayer, "abstraction layer")
                                        );
                                    }
                                    else
                                    {
                                        LOG_IF_FAILED(updateConfigHR);

                                        TraceLoggingWrite(
                                            MidiSrvTelemetryProvider::Provider(),
                                            __FUNCTION__,
                                            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                            TraceLoggingPointer(this, "this"),
                                            TraceLoggingHResult(updateConfigHR, "hresult"),
                                            TraceLoggingWideString(L"Failed to update configuration.", "message"),
                                            TraceLoggingGuid(AbstractionLayer, "abstraction layer")
                                        );
                                    }
                                }
                                else
                                {

                                    TraceLoggingWrite(
                                        MidiSrvTelemetryProvider::Provider(),
                                        __FUNCTION__,
                                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                        TraceLoggingPointer(this, "this"),
                                        TraceLoggingWideString(L"Configuration updated.", "message"),
                                        TraceLoggingGuid(AbstractionLayer, "abstraction layer")
                                    );
                                }

                            }


                        }
                    }
                    else
                    {
                        TraceLoggingWrite(
                            MidiSrvTelemetryProvider::Provider(),
                            __FUNCTION__,
                            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Transport abstraction configuration manager activation failed with null result.", "message"),
                            TraceLoggingGuid(AbstractionLayer, "abstraction layer")
                        );
                    }
                }
                else
                {
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        __FUNCTION__,
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingHResult(configHR, "hresult"),
                        TraceLoggingWideString(L"Transport abstraction configuration manager activation failed.", "message"),
                        TraceLoggingGuid(AbstractionLayer, "abstraction layer")
                    );
                }
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    __FUNCTION__,
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Transport Abstraction activation failed (nullptr return).", "message"),
                    TraceLoggingGuid(AbstractionLayer, "abstraction layer")
                );
            }

        }
        catch (...)
        {
            // TODO: Log exception
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Exception loading transport abstraction.", "message"),
                TraceLoggingGuid(AbstractionLayer, "abstraction layer")
            );

        }
    }

    m_PerformanceManager = PerformanceManager;

    return S_OK;
}

typedef struct _CREATECONTEXT
{
    PMIDIPORT MidiPort{ nullptr };
    wil::unique_event creationCompleted{wil::EventOptions::None};
    DEVPROPERTY* InterfaceDevProperties{ nullptr };
    ULONG IntPropertyCount{ 0 };
} CREATECONTEXT, * PCREATECONTEXT;

typedef struct _PARENTDEVICECREATECONTEXT
{
    PMIDIPARENTDEVICE MidiParentDevice{ nullptr };
    wil::unique_event CreationCompleted{ wil::EventOptions::None };
} PARENTDEVICECREATECONTEXT, * PPARENTDEVICECREATECONTEXT;



void SwMidiPortCreateCallback(__in HSWDEVICE hSwDevice, __in HRESULT CreationResult, __in PVOID pContext, __in_opt PCWSTR /* pszDeviceInstanceId */)
{
    // fix code analysis complaint
    if (pContext == nullptr) return;

    PCREATECONTEXT creationContext = (PCREATECONTEXT)pContext;

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingWideString(creationContext->MidiPort->InstanceId.c_str(), "instance id"),
        TraceLoggingULong(creationContext->IntPropertyCount, "property count")
    );


    // interface registration has started, assume
    // failure
    creationContext->MidiPort->SwDeviceState = SWDEVICESTATE::Failed;
    creationContext->MidiPort->hr = CreationResult;

    if (SUCCEEDED(creationContext->MidiPort->hr))
    {
        creationContext->MidiPort->hr = SwDeviceInterfaceRegister(
            hSwDevice,
            creationContext->MidiPort->InterfaceCategory,
            nullptr,
            creationContext->IntPropertyCount,
            creationContext->InterfaceDevProperties,
            TRUE,
            wil::out_param(creationContext->MidiPort->DeviceInterfaceId));

        // TODO: Is the string output parameter here leaking? Docs say you need
        // to free it using SwMemFree. Makes me thing we should copy it and then
        // free it up.

    }

    if (SUCCEEDED(creationContext->MidiPort->hr))
    {
        // success, mark the port as created
        creationContext->MidiPort->SwDeviceState = SWDEVICESTATE::Created;
    }

    // success or failure, signal we have completed.
    creationContext->creationCompleted.SetEvent();
}


void
SwMidiParentDeviceCreateCallback(
    __in HSWDEVICE /*hSwDevice*/,
    __in HRESULT CreationResult,
    __in_opt PVOID pContext,
    __in_opt PCWSTR pszDeviceInstanceId
)
{
    if (pContext == nullptr)
    {
        // TODO: Should log this.

        return;
    }

    PPARENTDEVICECREATECONTEXT creationContext = (PPARENTDEVICECREATECONTEXT)pContext;

    // interface registration has started, assume failure
    creationContext->MidiParentDevice->SwDeviceState = SWDEVICESTATE::Failed;
    creationContext->MidiParentDevice->hr = CreationResult;

    LOG_IF_FAILED(CreationResult);

    if (SUCCEEDED(CreationResult))
    {
        // success, mark the port as created
        creationContext->MidiParentDevice->SwDeviceState = SWDEVICESTATE::Created;

        // get the new device instance ID. This is usually modified from what we started with
        creationContext->MidiParentDevice->InstanceId = std::wstring(pszDeviceInstanceId);
    }

    // success or failure, signal we have completed.
    creationContext->CreationCompleted.SetEvent();
}

_Use_decl_annotations_
HRESULT
CMidiDeviceManager::ActivateVirtualParentDevice
(
    ULONG DevPropertyCount,
    PVOID DevProperties,
    PVOID CreateInfo,
    PWSTR CreatedSwDeviceId,
    ULONG CreatedSwDeviceIdCharCount
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    RETURN_HR_IF_NULL(E_INVALIDARG, CreateInfo);
    RETURN_HR_IF_NULL(E_INVALIDARG, CreatedSwDeviceId);
    RETURN_HR_IF(E_INVALIDARG, CreatedSwDeviceIdCharCount == 0);

    if (CreatedSwDeviceId != nullptr && CreatedSwDeviceIdCharCount > 0)
    {
        CreatedSwDeviceId[CreatedSwDeviceIdCharCount - 1] = (wchar_t)0;
    }

    auto pcreateinfo = (SW_DEVICE_CREATE_INFO *)CreateInfo;

    std::unique_ptr<MIDIPARENTDEVICE> midiParent = std::make_unique<MIDIPARENTDEVICE>();
    RETURN_IF_NULL_ALLOC(midiParent);

    midiParent->SwDeviceState = SWDEVICESTATE::CreatePending;

    PARENTDEVICECREATECONTEXT creationContext;
    creationContext.MidiParentDevice = midiParent.get();

    std::vector<DEVPROPERTY> devProperties{};

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;

    if (DevPropertyCount > 0 && DevProperties != nullptr)
    {
        for (ULONG i = 0; i < DevPropertyCount; i++)
        {
            devProperties.push_back(((DEVPROPERTY*)DevProperties)[i]);
        }
    }

    devProperties.push_back({ {DEVPKEY_Device_PresenceNotForDevice, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue });

    devProperties.push_back({ {DEVPKEY_Device_NoConnectSound, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)),&devPropTrue });


    RETURN_IF_FAILED(SwDeviceCreate(
        MIDI_DEVICE_ENUMERATOR,                 
        MIDI_SWD_VIRTUAL_PARENT_ROOT,           // root device
        pcreateinfo,
        (ULONG)devProperties.size(),            // count of properties
        (DEVPROPERTY*)devProperties.data(),     // pointer to properties
        SwMidiParentDeviceCreateCallback,       // callback
        &creationContext,
        wil::out_param(creationContext.MidiParentDevice->SwDevice)));

    // wait 5 seconds for creation to complete
    creationContext.CreationCompleted.wait(5000);

    // confirm we were able to register the interface
    RETURN_IF_FAILED(creationContext.MidiParentDevice->hr);
    RETURN_HR_IF(E_FAIL, creationContext.MidiParentDevice->SwDeviceState != SWDEVICESTATE::Created);


    // return the created device interface Id. This is needed for anything that will 
    // do a match in the Ids from Windows.Devices.Enumeration
    if (CreatedSwDeviceId != nullptr && CreatedSwDeviceIdCharCount > 0)
    {
        creationContext.MidiParentDevice->InstanceId.copy(CreatedSwDeviceId, CreatedSwDeviceIdCharCount - 1);

        CreatedSwDeviceId[CreatedSwDeviceIdCharCount - 1] = (wchar_t)0;

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(CreatedSwDeviceId)
        );

    }

    if (SUCCEEDED(creationContext.MidiParentDevice->hr))
    {
        m_MidiParents.push_back(std::move(midiParent));
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiDeviceManager::ActivateEndpoint
(
    PCWSTR ParentInstanceId,
    BOOL UMPOnly,
    MidiFlow MidiFlow,
    PMIDIENDPOINTCOMMONPROPERTIES CommonProperties,
    ULONG IntPropertyCount,
    ULONG DevPropertyCount,
    PVOID InterfaceDevProperties,
    PVOID DeviceDevProperties,
    PVOID CreateInfo,
    PWSTR CreatedDeviceInterfaceId,
    ULONG CreatedDeviceInterfaceIdWCharCount
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(ParentInstanceId, "parent"),
        TraceLoggingBool(CommonProperties != nullptr, "Common Properties Provided"),
        TraceLoggingULong(IntPropertyCount, "Interface Property Count"),
        TraceLoggingULong(DevPropertyCount, "Device Property Count"),
        TraceLoggingBool(UMPOnly, "UMP Only")
    );

    auto lock = m_MidiPortsLock.lock();

    const bool alreadyActivated = std::find_if(m_MidiPorts.begin(), m_MidiPorts.end(), [&](const std::unique_ptr<MIDIPORT>& Port)
    {
        // if this instance id already activated, then we cannot activate/create a second time,
        if (((SW_DEVICE_CREATE_INFO*)CreateInfo)->pszInstanceId == Port->InstanceId)
        {
            return true;
        }

        return false;
    }) != m_MidiPorts.end();

    if (CreatedDeviceInterfaceId != nullptr && CreatedDeviceInterfaceIdWCharCount > 0)
    {
        // ensure null termination and clear any garbage
        memset(CreatedDeviceInterfaceId, 0, CreatedDeviceInterfaceIdWCharCount * sizeof(wchar_t));
        //CreatedDeviceInterfaceId[CreatedDeviceInterfaceIdWCharCount - 1] = (wchar_t)0;
    }


    if (alreadyActivated)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(ParentInstanceId, "parent"),
            TraceLoggingWideString(L"Already Activated", "message")
        );

        return S_FALSE;
    }
    else
    {
        std::vector<DEVPROPERTY> allInterfaceProperties{};

        if (IntPropertyCount > 0 && InterfaceDevProperties != nullptr)
        {
            // copy the incoming array into a vector so that we can add additional items.
            std::vector<DEVPROPERTY> suppliedInterfaceProperties((DEVPROPERTY*)InterfaceDevProperties, (DEVPROPERTY*)(InterfaceDevProperties) + IntPropertyCount);

            // copy to the main vector that we're going to keep using.
            allInterfaceProperties = suppliedInterfaceProperties;


            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(ParentInstanceId, "parent"),
                TraceLoggingULong((ULONG)IntPropertyCount, "provided interface props count"),
                TraceLoggingULong((ULONG)allInterfaceProperties.size(), "copied interface props count")
            );
        }

        DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
        DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;

        // Build common properties so these are consistent from endpoint to endpoint
        if (CommonProperties != nullptr)
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(ParentInstanceId, "parent"),
                TraceLoggingWideString(L"Common properties object supplied", "message")
            );


            allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_AbstractionLayer, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_GUID, (ULONG)(sizeof(GUID)), (PVOID)(&(CommonProperties->AbstractionLayerGuid)) });

            allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointDevicePurpose, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_UINT32, (ULONG)(sizeof(UINT32)), (PVOID)(&(CommonProperties->EndpointPurpose)) });
            
            if (CommonProperties->FriendlyName != nullptr && wcslen(CommonProperties->FriendlyName) > 0)
            {
                allInterfaceProperties.push_back(DEVPROPERTY{ {DEVPKEY_DeviceInterface_FriendlyName, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (wcslen(CommonProperties->FriendlyName) + 1)), (PVOID)(CommonProperties->FriendlyName) });
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    __FUNCTION__,
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(ParentInstanceId),
                    TraceLoggingWideString(L"Common Properties: Friendly Name is null or empty", "message")
                );

                return E_INVALIDARG;
            }

            // transport information

            if (CommonProperties->TransportMnemonic != nullptr && wcslen(CommonProperties->TransportMnemonic) > 0)
            {
                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_TransportMnemonic, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (wcslen(CommonProperties->TransportMnemonic) + 1)), (PVOID)CommonProperties->TransportMnemonic });
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    __FUNCTION__,
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(ParentInstanceId),
                    TraceLoggingWideString(L"Common Properties: Transport Mnemonic is null or empty", "message")
                );

                return E_INVALIDARG;
            }


            if (CommonProperties->TransportSuppliedEndpointName != nullptr && wcslen(CommonProperties->TransportSuppliedEndpointName) > 0)
            {
                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_TransportSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (wcslen(CommonProperties->TransportSuppliedEndpointName) + 1)), (PVOID)CommonProperties->TransportSuppliedEndpointName });
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    __FUNCTION__,
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(ParentInstanceId),
                    TraceLoggingWideString(L"Clearing PKEY_MIDI_TransportSuppliedEndpointName", "message")
                );
                
                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_TransportSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
            }

            if (CommonProperties->TransportSuppliedEndpointDescription != nullptr && wcslen(CommonProperties->TransportSuppliedEndpointDescription) > 0)
            {
                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_TransportSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr},
                   DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (wcslen(CommonProperties->TransportSuppliedEndpointDescription) + 1)), (PVOID)CommonProperties->TransportSuppliedEndpointDescription });
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    __FUNCTION__,
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(ParentInstanceId),
                    TraceLoggingWideString(L"Clearing PKEY_MIDI_TransportSuppliedDescription", "message")
                );

                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_TransportSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
            }

            if (CommonProperties->UniqueIdentifier != nullptr && wcslen(CommonProperties->UniqueIdentifier) > 0)
            {
                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_SerialNumber, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (wcslen(CommonProperties->UniqueIdentifier) + 1)), (PVOID)CommonProperties->UniqueIdentifier });
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    __FUNCTION__,
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(ParentInstanceId),
                    TraceLoggingWideString(L"Clearing PKEY_MIDI_SerialNumber", "message")
                );

                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_SerialNumber, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
            }

            if (CommonProperties->ManufacturerName != nullptr && wcslen(CommonProperties->ManufacturerName) != 0)
            {
                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_ManufacturerName, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (wcslen(CommonProperties->ManufacturerName) + 1)), (PVOID)CommonProperties->ManufacturerName });
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    __FUNCTION__,
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(ParentInstanceId),
                    TraceLoggingWideString(L"Clearing PKEY_MIDI_ManufacturerName", "message")
                );

                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_ManufacturerName, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
            }

            // user-supplied information, if available

            if (CommonProperties->UserSuppliedEndpointName != nullptr && wcslen(CommonProperties->UserSuppliedEndpointName) != 0)
            {
                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_UserSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (wcslen(CommonProperties->UserSuppliedEndpointName) + 1)), (PVOID)CommonProperties->UserSuppliedEndpointName });
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    __FUNCTION__,
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(ParentInstanceId),
                    TraceLoggingWideString(L"Clearing PKEY_MIDI_UserSuppliedEndpointName", "message")
                );

                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_UserSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
            }

            if (CommonProperties->UserSuppliedEndpointDescription != nullptr && wcslen(CommonProperties->UserSuppliedEndpointDescription) != 0)
            {
                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_UserSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (wcslen(CommonProperties->UserSuppliedEndpointDescription) + 1)), (PVOID)CommonProperties->UserSuppliedEndpointDescription });
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    __FUNCTION__,
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(ParentInstanceId),
                    TraceLoggingWideString(L"Clearing PKEY_MIDI_UserSuppliedDescription", "message")
                );

                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_UserSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
            }

            // TEMP
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(ParentInstanceId, "parent"),
                TraceLoggingWideString(L"Adding other non-string properties", "message")
            );

            // data format. These each have different uses. Native format is the device's format. Supported
            // format is how we talk to it from the service (the driver may translate to UMP, for example)

            allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_NativeDataFormat, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BYTE, (ULONG)(sizeof(BYTE)), (PVOID)(&(CommonProperties->NativeDataFormat)) });

            allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_SupportedDataFormats, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_UINT32, (ULONG)(sizeof(UINT32)), (PVOID)(&(CommonProperties->SupportedDataFormats)) });

            // Protocol defaults for cases when protocol negotiation may not happen or may not complete

            allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointSupportsMidi1Protocol, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BOOLEAN, (ULONG)(sizeof(DEVPROP_BOOLEAN)), (PVOID)(CommonProperties->SupportsMidi1ProtocolDefaultValue ? &devPropTrue : &devPropFalse) });

            allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointSupportsMidi2Protocol, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BOOLEAN, (ULONG)(sizeof(DEVPROP_BOOLEAN)), (PVOID)(CommonProperties->SupportsMidi2ProtocolDefaultValue ? &devPropTrue : &devPropFalse) });

            // behavior

            allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_GenerateIncomingTimestamp, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BOOLEAN, (ULONG)(sizeof(DEVPROP_BOOLEAN)), (PVOID)(CommonProperties->GenerateIncomingTimestamps ? &devPropTrue : &devPropFalse) });

            allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointRequiresMetadataHandler, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BOOLEAN, (ULONG)(sizeof(DEVPROP_BOOLEAN)), (PVOID)(CommonProperties->RequiresMetadataHandler ? &devPropTrue : &devPropFalse) });

            allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_SupportsMulticlient, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BOOLEAN, (ULONG)(sizeof(DEVPROP_BOOLEAN)), (PVOID)(CommonProperties->SupportsMultiClient ? &devPropTrue : &devPropFalse) });
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(ParentInstanceId, "parent"),
                TraceLoggingWideString(L"NO Common properties object supplied", "message")
            );

        }

        // TEMP
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(ParentInstanceId, "parent"),
            TraceLoggingWideString(L"Adding clearing properties", "message")
        );

        // Clear almost all of the protocol negotiation-discovered properties. We keep the "supports MIDI 1.0" and "supports MIDI 2.0" 
        // defaulted based on the native transport type of the device as per above.

        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointProvidedName, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointProvidedNameLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointProvidedProductInstanceId, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointProvidedProductInstanceIdLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });

        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointConfiguredProtocol, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointConfiguredToReceiveJRTimestamps, DEVPROP_STORE_SYSTEM, nullptr},DEVPROP_TYPE_EMPTY, 0, nullptr });
        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointConfiguredToSendJRTimestamps, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointConfigurationLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });

        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointSupportsReceivingJRTimestamps, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointSupportsSendingJRTimestamps, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointUmpVersionMajor, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointUmpVersionMinor, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointInformationLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });

        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_DeviceIdentity, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_DeviceIdentityLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });

        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_FunctionBlocksAreStatic, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_FunctionBlockCount, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_FunctionBlocksLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });

        // remove actual function blocks and their names

        //// TEMP
        //TraceLoggingWrite(
        //    MidiSrvTelemetryProvider::Provider(),
        //    __FUNCTION__,
        //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        //    TraceLoggingPointer(this, "this"),
        //    TraceLoggingWideString(ParentInstanceId, "parent"),
        //    TraceLoggingWideString(L"Adding clearing properties for function blocks", "message")
        //);

        //for (int i = 0; i < MIDI_MAX_FUNCTION_BLOCKS; i++)
        //{
        //    // clear out the function block data
        //    DEVPROPKEY fbkey;
        //    fbkey.fmtid = PKEY_MIDI_FunctionBlock00.fmtid;    // hack
        //    fbkey.pid = MIDI_FUNCTION_BLOCK_PROPERTY_INDEX_START + i;

        //    allInterfaceProperties.push_back(DEVPROPERTY{ {fbkey, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });

        //    // clear out the function block name data
        //    DEVPROPKEY namekey;
        //    namekey.fmtid = PKEY_MIDI_FunctionBlockName00.fmtid;    // hack
        //    namekey.pid = MIDI_FUNCTION_BLOCK_NAME_PROPERTY_INDEX_START + i;

        //    allInterfaceProperties.push_back(DEVPROPERTY{ {namekey, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
        //}

        // TEMP
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(ParentInstanceId, "parent"),
            TraceLoggingWideString(L"Registering cleanupOnFailure", "message")
        );


        std::wstring deviceInterfaceId{ };

        auto cleanupOnFailure = wil::scope_exit([&]()
        {
            // in the event that creation fails, clean up all of the interfaces associated with this InstanceId
            DeactivateEndpoint(((SW_DEVICE_CREATE_INFO*)CreateInfo)->pszInstanceId);
        });


        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(ParentInstanceId),
            TraceLoggingWideString(L"Activating UMP Endpoint", "message")
        );
        
        // Activate the UMP version of this endpoint.
        auto activationResult = ActivateEndpointInternal(
            ParentInstanceId, 
            nullptr, 
            FALSE, 
            MidiFlow, 
            (ULONG)allInterfaceProperties.size(),
            DevPropertyCount, 
            (DEVPROPERTY*)(allInterfaceProperties.data()),
            (DEVPROPERTY*)DeviceDevProperties, 
            (SW_DEVICE_CREATE_INFO*)CreateInfo, 
            &deviceInterfaceId);

        if (FAILED(activationResult))
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(ParentInstanceId),
                TraceLoggingHResult(activationResult, "Activation HRESULT"),
                TraceLoggingWideString(L"Failed to activate UMP endpoint", "message")
            );

            RETURN_IF_FAILED(activationResult);
        }


        if (!UMPOnly)
        {
            // now activate the midi 1 SWD's for this endpoint
            if (MidiFlow == MidiFlowBidirectional)
            {
                // This logic will need to change. We need one endpoint for each group, but we
                // don't know that until we get the function blocks. We know a little due to
                // the Group Terminal Blocks in the case of USB, but that's only for one transport
                // 
                // It may be easier to have the abstraction create the MIDI 1.0 SWDs by calling
                // functions when it's ready? That way it can get the info it needs first, and then
                // create the MIDI 1.0 interfaces. Need to think through that flow.


                // if this is a bidirectional endpoint, it gets an in and an out midi 1 swd's.
                RETURN_IF_FAILED(ActivateEndpointInternal(
                    ParentInstanceId, 
                    deviceInterfaceId.c_str(), 
                    TRUE, 
                    MidiFlowOut, 
                    (ULONG)allInterfaceProperties.size(), 
                    DevPropertyCount, 
                    (DEVPROPERTY*)(allInterfaceProperties.data()), 
                    (DEVPROPERTY*)DeviceDevProperties, 
                    (SW_DEVICE_CREATE_INFO*)CreateInfo, 
                    nullptr));

                RETURN_IF_FAILED(ActivateEndpointInternal(
                    ParentInstanceId, 
                    deviceInterfaceId.c_str(), 
                    TRUE, 
                    MidiFlowIn, 
                    (ULONG)allInterfaceProperties.size(), 
                    DevPropertyCount, 
                    (DEVPROPERTY*)(allInterfaceProperties.data()), 
                    (DEVPROPERTY*)DeviceDevProperties, 
                    (SW_DEVICE_CREATE_INFO*)CreateInfo, 
                    nullptr));
            }
            else
            {
                RETURN_IF_FAILED(ActivateEndpointInternal(
                    ParentInstanceId, 
                    deviceInterfaceId.c_str(), 
                    TRUE, 
                    MidiFlow, 
                    (ULONG)allInterfaceProperties.size(),
                    DevPropertyCount,
                    (DEVPROPERTY*)(allInterfaceProperties.data()),
                    (DEVPROPERTY*)DeviceDevProperties,
                    (SW_DEVICE_CREATE_INFO*)CreateInfo, 
                    nullptr));
            }
        }

        // return the created device interface Id. This is needed for anything that will 
        // do a match in the Ids from Windows.Devices.Enumeration
        if (CreatedDeviceInterfaceId != nullptr && CreatedDeviceInterfaceIdWCharCount > 0)
        {
            //memset((byte*)CreatedDeviceInterfaceId, 0, CreatedDeviceInterfaceIdBufferSize * sizeof(wchar_t));
            deviceInterfaceId.copy(CreatedDeviceInterfaceId, CreatedDeviceInterfaceIdWCharCount - 1);

            CreatedDeviceInterfaceId[CreatedDeviceInterfaceIdWCharCount - 1] = (wchar_t)0;
        }

        cleanupOnFailure.release();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiDeviceManager::ActivateEndpointInternal
(
    PCWSTR ParentInstanceId,
    PCWSTR AssociatedInstanceId,
    BOOL MidiOne,
    MidiFlow MidiFlow,
    ULONG IntPropertyCount,
    ULONG DevPropertyCount,
    DEVPROPERTY *InterfaceDevProperties,
    DEVPROPERTY *DeviceDevProperties,
    SW_DEVICE_CREATE_INFO *CreateInfo,
    std::wstring *DeviceInterfaceId
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, InterfaceDevProperties);
    RETURN_HR_IF(E_INVALIDARG, IntPropertyCount == 0);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(ParentInstanceId, "parent instance id"),
        TraceLoggingWideString(AssociatedInstanceId, "associated instance id"),
        TraceLoggingBool(MidiOne, "midi 1"),
        TraceLoggingULong(IntPropertyCount, "interface prop count"),
        TraceLoggingULong(DevPropertyCount, "device prop count")
    );


    std::unique_ptr<MIDIPORT> midiPort = std::make_unique<MIDIPORT>();

    RETURN_IF_NULL_ALLOC(midiPort);

    std::vector<DEVPROPERTY> interfaceProperties{};

    // copy the incoming array into a vector so that we can add additional items.
    if (InterfaceDevProperties != nullptr && IntPropertyCount > 0)
    {
        std::vector<DEVPROPERTY> existingInterfaceProperties(InterfaceDevProperties, InterfaceDevProperties + IntPropertyCount);

        // copy 'em over
        interfaceProperties = existingInterfaceProperties;
    }

    if (AssociatedInstanceId != nullptr && wcslen(AssociatedInstanceId) > 0)
    {
        // add any additional required items to the vector
        interfaceProperties.push_back({ {PKEY_MIDI_AssociatedUMP, DEVPROP_STORE_SYSTEM, nullptr},
                   DEVPROP_TYPE_STRING, static_cast<ULONG>((wcslen(AssociatedInstanceId) + 1) * sizeof(WCHAR)), (PVOID)AssociatedInstanceId });
    }
    else
    {
        // necessary for cases where this was previously set and cached

        interfaceProperties.push_back({ {PKEY_MIDI_AssociatedUMP, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
    }


    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;

    interfaceProperties.push_back({ {DEVPKEY_Device_PresenceNotForDevice, DEVPROP_STORE_SYSTEM, nullptr},
        DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue });

    interfaceProperties.push_back({ {DEVPKEY_Device_NoConnectSound, DEVPROP_STORE_SYSTEM, nullptr},
        DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue });


    midiPort->InstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(CreateInfo->pszInstanceId);
    midiPort->MidiFlow = MidiFlow;
    midiPort->Enumerator = MidiOne ? AUDIO_DEVICE_ENUMERATOR : MIDI_DEVICE_ENUMERATOR;

    if (MidiOne)
    {
        if (MidiFlow == MidiFlow::MidiFlowOut)
        {
            midiPort->InterfaceCategory = &DEVINTERFACE_MIDI_OUTPUT;
        }
        else if (MidiFlow == MidiFlow::MidiFlowIn)
        {
            midiPort->InterfaceCategory = &DEVINTERFACE_MIDI_INPUT;
        }
        else
        {
            RETURN_IF_FAILED(E_INVALIDARG);
        }
    }
    else
    {
        if (MidiFlow == MidiFlow::MidiFlowOut)
        {
            midiPort->InterfaceCategory = &DEVINTERFACE_UNIVERSALMIDIPACKET_OUTPUT;
        }
        else if (MidiFlow == MidiFlow::MidiFlowIn)
        {
            midiPort->InterfaceCategory = &DEVINTERFACE_UNIVERSALMIDIPACKET_INPUT;
        }
        else if (MidiFlow == MidiFlow::MidiFlowBidirectional)
        {
            midiPort->InterfaceCategory = &DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI;
        }
        else
        {
            RETURN_IF_FAILED(E_INVALIDARG);
        }
    }

    // lambdas can only be converted to a function pointer if they
    // don't do capture, so copy everything into the CREATECONTEXT
    // to share with the SwDeviceCreate callback.
    CREATECONTEXT creationContext;
    creationContext.MidiPort = midiPort.get();
    creationContext.InterfaceDevProperties = interfaceProperties.data();
    creationContext.IntPropertyCount = (ULONG)interfaceProperties.size();

    // create the devnode for the device
    midiPort->hr = SwDeviceCreate(
        midiPort->Enumerator.c_str(),
        ParentInstanceId,
        CreateInfo,
        DevPropertyCount,
        DevPropertyCount == 0 ? (DEVPROPERTY*)nullptr : DeviceDevProperties,
        SwMidiPortCreateCallback,
        &creationContext,
        wil::out_param(midiPort->SwDevice));

    if (SUCCEEDED(midiPort->hr))
    {
        // wait 10 seconds for creation to complete
        creationContext.creationCompleted.wait(10000);
    }
    else if (HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS) == midiPort->hr)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Already exists"),
            TraceLoggingWideString(ParentInstanceId),
            TraceLoggingBool(MidiOne)
        );

        // if the devnode already exists, then we likely only need to create/activate the interface, a previous
        // call created the devnode. First, locate the matching SwDevice handle for the existing devnode.
        auto item = std::find_if(m_MidiPorts.begin(), m_MidiPorts.end(), [&](const std::unique_ptr<MIDIPORT>& Port)
        {
            // if this instance id already activated, then we cannot activate/create a second time,
            if (midiPort->InstanceId == Port->InstanceId &&
                midiPort->Enumerator == Port->Enumerator)
            {
                return true;
            }

            return false;
        });

        if (item != m_MidiPorts.end())
        {
            SwMidiPortCreateCallback(item->get()->SwDevice.get(), S_OK, &creationContext, nullptr);
        }
    }
    else
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"SwDeviceCreate failed", "message"),
            TraceLoggingWideString(ParentInstanceId),
            TraceLoggingBool(MidiOne)
        );
    }

    // confirm we were able to register the interface

    if (FAILED(midiPort->hr))
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingHResult(midiPort->hr, "hresult"),
            TraceLoggingWideString(L"SwDeviceCreate returned a failed HR", "message"),
            TraceLoggingWideString(ParentInstanceId),
            TraceLoggingBool(MidiOne)
        );
    }
    RETURN_IF_FAILED(midiPort->hr);

    if (midiPort->SwDeviceState != SWDEVICESTATE::Created)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"SwDeviceState != SWDEVICESTATE::Created", "message"),
            TraceLoggingWideString(ParentInstanceId),
            TraceLoggingBool(MidiOne)
        );
    }
    RETURN_HR_IF(E_FAIL, midiPort->SwDeviceState != SWDEVICESTATE::Created);

    if (DeviceInterfaceId)
    {
        *DeviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(midiPort->DeviceInterfaceId.get());

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(DeviceInterfaceId->c_str(), "created device interface id")
        );
    }

    // success, transfer the midiPort to the list
    m_MidiPorts.push_back(std::move(midiPort));

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiDeviceManager::UpdateEndpointProperties
(
    PCWSTR DeviceInterfaceId,
    ULONG IntPropertyCount,
    PVOID InterfaceDevProperties
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, DeviceInterfaceId);
    RETURN_HR_IF_NULL(E_INVALIDARG, InterfaceDevProperties);
    RETURN_HR_IF(E_INVALIDARG, IntPropertyCount == 0);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(DeviceInterfaceId, "device interface id"),
        TraceLoggingULong(IntPropertyCount, "property count")
    );

    auto requestedInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(DeviceInterfaceId);

    // locate the MIDIPORT 
    auto item = std::find_if(m_MidiPorts.begin(), m_MidiPorts.end(), [&](const std::unique_ptr<MIDIPORT>& Port)
        {
            auto portInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(Port->DeviceInterfaceId.get());

            return (portInterfaceId == requestedInterfaceId);
        });

    if (item == m_MidiPorts.end())
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Device not found. If the updates are from the config file, the device may not yet have been enumerated.", "message"),
            TraceLoggingWideString(DeviceInterfaceId, "device interface id"),
            TraceLoggingULong(IntPropertyCount, "property count")
        );

        // device not found
        return E_NOTFOUND;
    }
    else
    {
        // Using the found handle, add/update properties
        auto deviceHandle = item->get()->SwDevice.get();

        auto propSetHR = SwDeviceInterfacePropertySet(
            deviceHandle,
            item->get()->DeviceInterfaceId.get(),
            IntPropertyCount,
            (const DEVPROPERTY*)InterfaceDevProperties
        );

        if (SUCCEEDED(propSetHR))
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Properties set", "message"),
                TraceLoggingWideString(DeviceInterfaceId, "device interface id"),
                TraceLoggingULong(IntPropertyCount, "property count")
            );

            return S_OK;
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingHResult(propSetHR, "hresult"),
                TraceLoggingWideString(L"Error setting properties", "message"),
                TraceLoggingWideString(DeviceInterfaceId, "device interface id"),
                TraceLoggingULong(IntPropertyCount, "property count")
            );

            return propSetHR;
        }
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiDeviceManager::DeleteEndpointProperties
(
    PCWSTR DeviceInterfaceId,
    ULONG IntPropertyCount,
    PVOID InterfaceDevPropKeys
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(DeviceInterfaceId),
        TraceLoggingULong(IntPropertyCount)
    );

    OutputDebugString(L"\n" __FUNCTION__ " ");


    // create list of dev properties from the keys

    // Call UpdateEndpointProperties

    auto keys = (const DEVPROPKEY*)InterfaceDevPropKeys;

    std::vector<DEVPROPERTY> properties;

    for (ULONG i = 0; i < IntPropertyCount; i++)
    {
        properties.push_back({{ keys[i], DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_EMPTY, 0, NULL });
    }

    return UpdateEndpointProperties(DeviceInterfaceId, (ULONG)properties.size(), (PVOID)properties.data());
}

_Use_decl_annotations_
HRESULT
CMidiDeviceManager::DeactivateEndpoint
(
        PCWSTR InstanceId
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(InstanceId, "instance id")
    );

    auto cleanId = internal::NormalizeDeviceInstanceIdWStringCopy(InstanceId);

    // there may be more than one SWD associated with this instance id, as we reuse
    // the instance id for the legacy SWD, it just has a different activator and InterfaceClass.
    do
    {
        // locate the MIDIPORT that identifies the swd
        // NOTE: This uses InstanceId, not the Device Interface Id
        auto item = std::find_if(m_MidiPorts.begin(), m_MidiPorts.end(), [&](const std::unique_ptr<MIDIPORT>& Port)
        {
           // OutputDebugString(__FUNCTION__ L" Checking against stored instance id: ");
           // OutputDebugString(Port->InstanceId.c_str());
                
            if (cleanId == Port->InstanceId)
            {
                return true;
            }

            return false;
        });

        // exit if the item was not found. We're done.
        if (item == m_MidiPorts.end())
        {
            break;
        }
        else
        {
            // Erasing this item from the list will free the unique_ptr and also trigger a SwDeviceClose on the item->SwDevice,
            // which will deactivate the device, done.
            m_MidiPorts.erase(item);
        }
    } while (TRUE);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiDeviceManager::RemoveEndpoint
(
    PCWSTR InstanceId 
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(InstanceId)
    );

    // first deactivate, to ensure it's removed from the tracking list
    LOG_IF_FAILED(DeactivateEndpoint(InstanceId));

    // TODO: Locate the device with this instance id using windows.devices.enumeration,
    // and delete it.

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiDeviceManager::UpdateAbstractionConfiguration
(
    GUID AbstractionId,
    LPCWSTR ConfigurationJson,
    BOOL IsFromConfigurationFile,
    BSTR* Response
)
{
    if (ConfigurationJson != nullptr)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingGuid(AbstractionId, "abstraction id"),
            TraceLoggingWideString(ConfigurationJson, "config json"),
            TraceLoggingBool(IsFromConfigurationFile, "from config file")
        );

        if (auto search = m_MidiAbstractionConfigurationManagers.find(AbstractionId); search != m_MidiAbstractionConfigurationManagers.end())
        {
            auto configManager = search->second;

            if (configManager)
            {
                return configManager->UpdateConfiguration(ConfigurationJson, IsFromConfigurationFile, Response);
            }
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingGuid(AbstractionId, "abstraction id"),
                TraceLoggingWideString(L"Failed to find the referenced abstraction by its GUID", "message")
            );

        }
    }
    else
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Json is null", "message")
        );
    }


    // failed to find the abstraction or its related endpoint manager
    return E_FAIL;
}


HRESULT
CMidiDeviceManager::Cleanup()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    m_MidiEndpointManagers.clear();
    m_MidiAbstractionConfigurationManagers.clear();

    m_MidiPorts.clear();

    m_MidiParents.clear();

    return S_OK;
}






