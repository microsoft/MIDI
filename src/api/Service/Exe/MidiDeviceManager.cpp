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
    m_ConfigurationManager = configurationManager;

    // Get the enabled abstraction layers from the registry
    std::vector<GUID> availableAbstractionLayers = m_ConfigurationManager->GetEnabledTransportAbstractionLayers();

    for (auto const& AbstractionLayer : availableAbstractionLayers)
    {
        wil::com_ptr_nothrow<IMidiAbstraction> midiAbstraction;
        wil::com_ptr_nothrow<IMidiEndpointManager> endpointManager;

        try
        {
            // provide the initial settings for these transports
            // TODO: Still need a way to send updates to them without restarting the service or the abstraction
            // but that requires a pipe all the way down to the client to send up that configuration block
            auto transportSettingsJson = m_ConfigurationManager->GetConfigurationForTransportAbstraction(AbstractionLayer);

            // changed these from a return-on-fail to just log, so we don't prevent service startup
            // due to one bad abstraction

            LOG_IF_FAILED(CoCreateInstance(AbstractionLayer, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiAbstraction)));

            if (midiAbstraction != nullptr)
            {
                LOG_IF_FAILED(midiAbstraction->Activate(__uuidof(IMidiEndpointManager), (void**)&endpointManager));

                if (endpointManager != nullptr)
                {
                    // need to do this to avoid an ambiguous IUnknown cast error
                    wil::com_ptr_nothrow<IMidiEndpointProtocolManagerInterface> protocolManager = EndpointProtocolManager.get();

                    auto initializeResult = endpointManager->Initialize((IUnknown*)this, (IUnknown*)protocolManager.get(), transportSettingsJson.c_str());

                    LOG_IF_FAILED(initializeResult);

                    if (SUCCEEDED(initializeResult))
                    {
                        OutputDebugString(__FUNCTION__ L": Transport Abstraction initialized.\n");
                        m_MidiEndpointManagers.emplace(AbstractionLayer, std::move(endpointManager));
                    }
                    else
                    {
                        OutputDebugString(__FUNCTION__ L": Transport Abstraction initialization FAILED.\n");
                    }

                }
                else
                {
                    OutputDebugString(__FUNCTION__ L": Transport Abstraction Endpoint Manager activation failed (nullptr return).\n");
                }
            }
            else
            {
                OutputDebugString(__FUNCTION__ L": Transport Abstraction activation failed (nullptr return).\n");
            }
        }
        catch (...)
        {
            // TODO: Log
            OutputDebugString(__FUNCTION__ L": Exception loading transport abstraction.\n");
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
    OutputDebugString(L"" __FUNCTION__);

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
        OutputDebugString(__FUNCTION__ L" - CreationResult Success\n");

        // success, mark the port as created
        creationContext->MidiParentDevice->SwDeviceState = SWDEVICESTATE::Created;

        // get the new device instance ID. This is usually modified from what we started with
        creationContext->MidiParentDevice->InstanceId = std::wstring(pszDeviceInstanceId);
    }
    else
    {
        OutputDebugString(__FUNCTION__ L" - CreationResult FAILURE\n");
    }

    // success or failure, signal we have completed.
    creationContext->CreationCompleted.SetEvent();

    OutputDebugString(__FUNCTION__ L" - Complete\n");
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
    OutputDebugString(L"" __FUNCTION__);

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



    OutputDebugString(__FUNCTION__ L": Setting dev properties\n");

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



    OutputDebugString(__FUNCTION__ L": About to SwDeviceCreate\n");

    RETURN_IF_FAILED(SwDeviceCreate(
        MIDI_DEVICE_ENUMERATOR,                 
        MIDI_SWD_VIRTUAL_PARENT_ROOT,           // root device
        pcreateinfo,
        (ULONG)devProperties.size(),            // count of properties
        (DEVPROPERTY*)devProperties.data(),     // pointer to properties
        SwMidiParentDeviceCreateCallback,       // callback
        &creationContext,
        wil::out_param(creationContext.MidiParentDevice->SwDevice)));


    OutputDebugString(__FUNCTION__ L": About to wait for completion\n");

    // wait 5 seconds for creation to complete
    creationContext.CreationCompleted.wait(5000);


    OutputDebugString(__FUNCTION__ L": Done waiting. Checking for success or failure\n");

    // confirm we were able to register the interface
    RETURN_IF_FAILED(creationContext.MidiParentDevice->hr);
    RETURN_HR_IF(E_FAIL, creationContext.MidiParentDevice->SwDeviceState != SWDEVICESTATE::Created);


    OutputDebugString(__FUNCTION__ L": Getting the new CreatedSwDeviceId\n");

    // return the created device interface Id. This is needed for anything that will 
    // do a match in the Ids from Windows.Devices.Enumeration
    if (CreatedSwDeviceId != nullptr && CreatedSwDeviceIdCharCount > 0)
    {
        creationContext.MidiParentDevice->InstanceId.copy(CreatedSwDeviceId, CreatedSwDeviceIdCharCount - 1);

        CreatedSwDeviceId[CreatedSwDeviceIdCharCount - 1] = (wchar_t)0;

        OutputDebugString(CreatedSwDeviceId);
    }

    if (SUCCEEDED(creationContext.MidiParentDevice->hr))
    {
        OutputDebugString(__FUNCTION__ L": Success\n");

        m_MidiParents.push_back(std::move(midiParent));
    }

    OutputDebugString(__FUNCTION__ L": Complete\n");

    return S_OK;
}




_Use_decl_annotations_
HRESULT
CMidiDeviceManager::ActivateEndpoint
(
    PCWSTR ParentInstanceId,
    BOOL UMPOnly,
    MidiFlow MidiFlow,
    ULONG IntPropertyCount,
    ULONG DevPropertyCount,
    PVOID InterfaceDevProperties,
    PVOID DeviceDevProperties,
    PVOID CreateInfo,
    PWSTR CreatedDeviceInterfaceId,
    ULONG CreatedDeviceInterfaceIdCharCount
)
{
    OutputDebugString(__FUNCTION__ L": Enter\n");

    auto lock = m_MidiPortsLock.lock();

    OutputDebugString(__FUNCTION__ L": Checking for an existing port. \n");

    const bool alreadyActivated = std::find_if(m_MidiPorts.begin(), m_MidiPorts.end(), [&](const std::unique_ptr<MIDIPORT>& Port)
    {
        // if this instance id already activated, then we cannot activate/create a second time,
        if (((SW_DEVICE_CREATE_INFO*)CreateInfo)->pszInstanceId == Port->InstanceId)
        {
            return true;
        }

        return false;
    }) != m_MidiPorts.end();

    OutputDebugString(__FUNCTION__ L": About to Null terminate CreatedDeviceInterfaceId\n");

    if (CreatedDeviceInterfaceId != nullptr && CreatedDeviceInterfaceIdCharCount > 0)
    {
        CreatedDeviceInterfaceId[CreatedDeviceInterfaceIdCharCount - 1] = (wchar_t)0;
    }


    if (alreadyActivated)
    {
        return S_FALSE;
    }
    else
    {
        std::wstring deviceInterfaceId{ 0 };

        auto cleanupOnFailure = wil::scope_exit([&]()
        {
            // in the event that creation fails, clean up all of the interfaces associated with this InstanceId
            DeactivateEndpoint(((SW_DEVICE_CREATE_INFO*)CreateInfo)->pszInstanceId);
        });

        OutputDebugString(__FUNCTION__ L": About to ActivateEndpointInternal\n");

        // Activate the UMP version of this endpoint.
        RETURN_IF_FAILED(ActivateEndpointInternal(ParentInstanceId, nullptr, FALSE, MidiFlow, IntPropertyCount, DevPropertyCount, (DEVPROPERTY*)InterfaceDevProperties, (DEVPROPERTY*)DeviceDevProperties, (SW_DEVICE_CREATE_INFO*)CreateInfo, &deviceInterfaceId));

        if (!UMPOnly)
        {
            // now activate the midi 1 SWD's for this endpoint
            if (MidiFlow == MidiFlowBidirectional)
            {
                // if this is a bidirectional endpoint, it gets an in and an out midi 1 swd's.
                RETURN_IF_FAILED(ActivateEndpointInternal(ParentInstanceId, deviceInterfaceId.c_str(), TRUE, MidiFlowOut, IntPropertyCount, DevPropertyCount, (DEVPROPERTY*)InterfaceDevProperties, (DEVPROPERTY*)DeviceDevProperties, (SW_DEVICE_CREATE_INFO*)CreateInfo, nullptr));
                RETURN_IF_FAILED(ActivateEndpointInternal(ParentInstanceId, deviceInterfaceId.c_str(), TRUE, MidiFlowIn, IntPropertyCount, DevPropertyCount, (DEVPROPERTY*)InterfaceDevProperties, (DEVPROPERTY*)DeviceDevProperties, (SW_DEVICE_CREATE_INFO*)CreateInfo, nullptr));
            }
            else
            {
                RETURN_IF_FAILED(ActivateEndpointInternal(ParentInstanceId, deviceInterfaceId.c_str(), TRUE, MidiFlow, IntPropertyCount, DevPropertyCount, (DEVPROPERTY*)InterfaceDevProperties, (DEVPROPERTY*)DeviceDevProperties, (SW_DEVICE_CREATE_INFO*)CreateInfo, nullptr));
            }
        }

        OutputDebugString(__FUNCTION__ L": About to get CreatedDeviceInterfaceId\n");

        // return the created device interface Id. This is needed for anything that will 
        // do a match in the Ids from Windows.Devices.Enumeration
        if (CreatedDeviceInterfaceId != nullptr && CreatedDeviceInterfaceIdCharCount > 0)
        {
            //memset((byte*)CreatedDeviceInterfaceId, 0, CreatedDeviceInterfaceIdBufferSize * sizeof(wchar_t));
            deviceInterfaceId.copy(CreatedDeviceInterfaceId, CreatedDeviceInterfaceIdCharCount - 1);

            CreatedDeviceInterfaceId[CreatedDeviceInterfaceIdCharCount - 1] = (wchar_t)0;
        }

        cleanupOnFailure.release();
    }


    OutputDebugString(__FUNCTION__ L": Complete\n");

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
    OutputDebugString(__FUNCTION__ L": Enter\nParentInstanceId: ");
    OutputDebugString(ParentInstanceId);


    std::unique_ptr<MIDIPORT> midiPort = std::make_unique<MIDIPORT>();

    RETURN_IF_NULL_ALLOC(midiPort);

    // copy the incoming array into a vector so that we can add additional items.
    std::vector<DEVPROPERTY> interfaceProperties (InterfaceDevProperties, InterfaceDevProperties + IntPropertyCount);

    if (AssociatedInstanceId)
    {
        // add any additional required items to the vector
        interfaceProperties.push_back({ {PKEY_MIDI_AssociatedUMP, DEVPROP_STORE_SYSTEM, nullptr},
                   DEVPROP_TYPE_STRING, static_cast<ULONG>((wcslen(AssociatedInstanceId) + 1) * sizeof(WCHAR)), (PVOID)AssociatedInstanceId });
    }


    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;

    interfaceProperties.push_back({ {DEVPKEY_Device_PresenceNotForDevice, DEVPROP_STORE_SYSTEM, nullptr},
        DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue });

    interfaceProperties.push_back({ {DEVPKEY_Device_NoConnectSound, DEVPROP_STORE_SYSTEM, nullptr},
        DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue });


    midiPort->InstanceId = internal::NormalizeDeviceInstanceIdCopy(CreateInfo->pszInstanceId);
    midiPort->MidiFlow = MidiFlow;
    midiPort->Enumerator = MidiOne?AUDIO_DEVICE_ENUMERATOR : MIDI_DEVICE_ENUMERATOR;

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

    OutputDebugString(__FUNCTION__ L": About to SwDeviceCreate\n");


    // create the devnode for the device
    midiPort->hr = SwDeviceCreate(
        midiPort->Enumerator.c_str(),
        ParentInstanceId,
        CreateInfo,
        DevPropertyCount,
        DeviceDevProperties,
        SwMidiPortCreateCallback,
        &creationContext,
        wil::out_param(midiPort->SwDevice));

    if (SUCCEEDED(midiPort->hr))
    {
        OutputDebugString(__FUNCTION__ L": SwDeviceCreate succeeded\n");

        // wait 10 seconds for creation to complete
        creationContext.creationCompleted.wait(10000);
    }
    else if (HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS) == midiPort->hr)
    {
        OutputDebugString(__FUNCTION__ L": SwDeviceCreate returned that the endpoint device already exists\n");

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

    // confirm we were able to register the interface
    RETURN_IF_FAILED(midiPort->hr);

    OutputDebugString(__FUNCTION__ L": Checking midiPort->SwDeviceState\n");

    RETURN_HR_IF(E_FAIL, midiPort->SwDeviceState != SWDEVICESTATE::Created);

    OutputDebugString(__FUNCTION__ L": Copying new DeviceInterfaceId\n");

    if (DeviceInterfaceId)
    {
        *DeviceInterfaceId = internal::NormalizeEndpointInterfaceIdCopy(midiPort->DeviceInterfaceId.get()).c_str();
    }

    // success, transfer the midiPort to the list
    m_MidiPorts.push_back(std::move(midiPort));

    OutputDebugString(__FUNCTION__ L": Complete\n");

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
    OutputDebugString(L"\n" __FUNCTION__ " ");

    auto requestedInterfaceId = internal::NormalizeEndpointInterfaceIdCopy(DeviceInterfaceId);

    //OutputDebugString(requestedInterfaceId.c_str());
    //OutputDebugString(L"\n");

    // locate the MIDIPORT 
    auto item = std::find_if(m_MidiPorts.begin(), m_MidiPorts.end(), [&](const std::unique_ptr<MIDIPORT>& Port)
        {
            auto portInterfaceId = internal::NormalizeEndpointInterfaceIdCopy(Port->DeviceInterfaceId.get());

//            OutputDebugString((L" -- Checking " + portInterfaceId).c_str());

            return (portInterfaceId == requestedInterfaceId);
        });

    if (item == m_MidiPorts.end())
    {
        OutputDebugString(__FUNCTION__ L" No matching device found\n");

        // device not found
        return E_FAIL;
    }
    else
    {
        OutputDebugString(__FUNCTION__ L" Found matching device in endpoint list\n");
        OutputDebugString(item->get()->DeviceInterfaceId.get());

        // Using the found handle, add/update properties
        auto deviceHandle = item->get()->SwDevice.get();

        RETURN_IF_FAILED(SwDeviceInterfacePropertySet(
            deviceHandle,
            item->get()->DeviceInterfaceId.get(),
            IntPropertyCount,
            (const DEVPROPERTY*)InterfaceDevProperties
        ));

        OutputDebugString(__FUNCTION__ L" Property updated successfully\n");
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
CMidiDeviceManager::DeleteAllEndpointInProtocolDiscoveredProperties
(
    PCWSTR DeviceInterfaceId
)
{
    // don't delete any keys that are specified by the user, or 
    // by the driver or transport. We're only deleting things that
    // are discovered in-protocol.

    std::vector<DEVPROPKEY> keys;

    keys.push_back(PKEY_MIDI_EndpointProvidedName);
    keys.push_back(PKEY_MIDI_EndpointProvidedProductInstanceId);

    keys.push_back(PKEY_MIDI_EndpointConfiguredProtocol);
    keys.push_back(PKEY_MIDI_EndpointConfiguredToReceiveJRTimestamps);
    keys.push_back(PKEY_MIDI_EndpointConfiguredToSendJRTimestamps);
    keys.push_back(PKEY_MIDI_EndpointSupportsMidi1Protocol);
    keys.push_back(PKEY_MIDI_EndpointSupportsMidi2Protocol);
    keys.push_back(PKEY_MIDI_EndpointSupportsReceivingJRTimestamps);
    keys.push_back(PKEY_MIDI_EndpointSupportsSendingJRTimestamps);
    keys.push_back(PKEY_MIDI_EndpointUmpVersionMajor);
    keys.push_back(PKEY_MIDI_EndpointUmpVersionMinor);

    keys.push_back(PKEY_MIDI_DeviceIdentity);

    keys.push_back(PKEY_MIDI_FunctionBlocksAreStatic);
    keys.push_back(PKEY_MIDI_FunctionBlockCount);

    // remove actual function blocks and their names

    for (int i = 0; i < MIDI_MAX_FUNCTION_BLOCKS; i++)
    {
        DEVPROPKEY fbkey;
        fbkey.fmtid = PKEY_MIDI_FunctionBlock00.fmtid;    // hack
        fbkey.pid = MIDI_FUNCTION_BLOCK_PROPERTY_INDEX_START + i;

        keys.push_back(fbkey);

        DEVPROPKEY namekey;
        namekey.fmtid = PKEY_MIDI_FunctionBlock00.fmtid;    // hack
        namekey.pid = MIDI_FUNCTION_BLOCK_NAME_PROPERTY_INDEX_START + i;

        keys.push_back(namekey);
    }

    return DeleteEndpointProperties(DeviceInterfaceId, (ULONG)keys.size(), (PVOID)keys.data());
}




_Use_decl_annotations_
HRESULT
CMidiDeviceManager::DeactivateEndpoint
(
        PCWSTR InstanceId
)
{
    OutputDebugString(__FUNCTION__ L" - enter. Cleaned instance id is: ");

    auto cleanId = internal::NormalizeDeviceInstanceIdCopy(InstanceId);

    OutputDebugString(cleanId.c_str());

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
            OutputDebugString(__FUNCTION__ L" - Id found. Removing SWD\n");

            // Erasing this item from the list will free the unique_ptr and also trigger a SwDeviceClose on the item->SwDevice,
            // which will deactivate the device, done.
            m_MidiPorts.erase(item);
        }
    } while (TRUE);

    OutputDebugString(__FUNCTION__ L" - exit\n");

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiDeviceManager::RemoveEndpoint
(
    PCWSTR InstanceId 
)
{
    // first deactivate, to ensure it's removed from the tracking list
    LOG_IF_FAILED(DeactivateEndpoint(InstanceId));

    // TODO: Locate the device with this instance id using windows.devices.enumeration,
    // and delete it.

    return S_OK;
}



HRESULT
CMidiDeviceManager::Cleanup()
{
    OutputDebugString(__FUNCTION__ L"\n");

    m_MidiEndpointManagers.clear();
    m_MidiPorts.clear();

    m_MidiParents.clear();

    return S_OK;
}






