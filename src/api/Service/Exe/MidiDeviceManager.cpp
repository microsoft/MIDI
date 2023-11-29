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
                    LOG_IF_FAILED(endpointManager->Initialize((IUnknown*)this, transportSettingsJson.c_str()));

                    m_MidiEndpointManagers.emplace(AbstractionLayer, std::move(endpointManager));

                    OutputDebugString(__FUNCTION__ L": Transport Abstraction activated.");
                }
                else
                {
                    OutputDebugString(__FUNCTION__ L": Transport Abstraction Endpoint Manager activation failed (nullptr return).");
                }
            }
            else
            {
                OutputDebugString(__FUNCTION__ L": Transport Abstraction activation failed (nullptr return).");
            }
        }
        catch (...)
        {
            // TODO: Log
            OutputDebugString(__FUNCTION__ L": Exception loading transport abstraction.");
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
    }

    if (SUCCEEDED(creationContext->MidiPort->hr))
    {
        // success, mark the port as created
        creationContext->MidiPort->SwDeviceState = SWDEVICESTATE::Created;
    }

    // success or failure, signal we have completed.
    creationContext->creationCompleted.SetEvent();
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

    //// get the MidiSrv filename so we can specify the icons
    //TCHAR szFileName[MAX_PATH];
    //GetModuleFileName(NULL, szFileName, MAX_PATH);

    //std::wstring interfaceIconPath(szFileName);
    //interfaceIconPath += L", " + std::to_wstring(IDI_ENDPOINT_DEVICE_ICON);

    //interfaceProperties.push_back({{PKEY_Devices_GlyphIcon, DEVPROP_STORE_SYSTEM, nullptr},
    //    DEVPROP_TYPE_STRING, static_cast<ULONG>(interfaceIconPath.length() + 1) * sizeof(WCHAR), (PVOID)interfaceIconPath.c_str()});


    midiPort->InstanceId = CreateInfo->pszInstanceId;
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
        // wait for creation to complete
        creationContext.creationCompleted.wait();
    }
    else if (HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS) == midiPort->hr)
    {
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
    RETURN_HR_IF(E_FAIL, midiPort->SwDeviceState != SWDEVICESTATE::Created);

    if (DeviceInterfaceId)
    {
        *DeviceInterfaceId = midiPort->DeviceInterfaceId.get();
    }

    // success, transfer the midiPort to the list
    m_MidiPorts.push_back(std::move(midiPort));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiDeviceManager::DeactivateEndpoint
(
        PCWSTR InstanceId
)
{
    // there may be more than one SWD associated with this instance id, as we reuse
    // the instance id for the legacy SWD, it just has a different activator and InterfaceClass.
    do
    {
        // locate the MIDIPORT that identifies the swd
        auto item = std::find_if(m_MidiPorts.begin(), m_MidiPorts.end(), [&](const std::unique_ptr<MIDIPORT>& Port)
        {
            // if this instance id already activated, then we cannot activate/create a second time,
            if (InstanceId == Port->InstanceId)
            {
                return true;
            }

            return false;
        });

        // if the item was found
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
    // first deactivate, to ensure it's removed from the tracking list
    LOG_IF_FAILED(DeactivateEndpoint(InstanceId));

    // TODO: Locate the device with this instance id using windows.devices.enumeration,
    // and delete it.

    return S_OK;
}

HRESULT
CMidiDeviceManager::Cleanup()
{
    m_MidiEndpointManagers.clear();
    m_MidiPorts.clear();

    return S_OK;
}

