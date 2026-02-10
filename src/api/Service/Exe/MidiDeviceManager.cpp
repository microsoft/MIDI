// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"
#include "ks.h"
#include "Midi2KSAggregateTransport.h"
#include "Midi2KSTransport.h"
#include "Midi2LoopbackMidiTransport.h"
#include "MidiEndpointNameTable.h"
#include "MidiClientManager.h"

#include "midi_ksa_pin_map_property.h"

#include "Feature_Servicing_MIDI2DeviceRemoval.h"

using namespace winrt::Windows::Devices::Enumeration;

const WCHAR driver32Path[]    = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Drivers32";
const WCHAR midisrvTransferComplete[] =  L"MidisrvTransferComplete";

const WCHAR szzMidiDeviceHardwareId[] =  L"MIDISRV\\MidiEndpoints" L"\0";
const WCHAR szzMidiDeviceCompatibleId[] =  L"GenericMidiEndpoint" L"\0";

// a cap to ensure we don't have runaway port numbers
#define MAX_WINMM_PORT_NUMBER 5000
#define MAX_WINMM_ENDPOINT_NAME_SIZE_WITHOUT_GROUP_SUFFIX (32-6)

#define MIDI_GROUP_TERMINAL_BLOCK_BIDIRECTIONAL 0x00
#define MIDI_GROUP_TERMINAL_BLOCK_INPUT         0x01
#define MIDI_GROUP_TERMINAL_BLOCK_OUTPUT        0x02

#define SWD_CREATION_TIMEOUT 10000

#pragma pack(push)
#pragma pack(1)
typedef struct
{
    WORD                            Size;
    BYTE                            Number;             // Group Terminal Block ID
    BYTE                            Direction;          // Group Terminal Block Type
    BYTE                            FirstGroupIndex;    // The first group in this block
    BYTE                            GroupCount;         // The number of groups spanned
    BYTE                            Protocol;           // The MIDI Protocol
    WORD                            MaxInputBandwidth;  ///< Maximum Input Bandwidth Capability in 4KB/second
    WORD                            MaxOutputBandwidth; ///< Maximum Output Bandwidth Capability in 4KB/second
} UMP_GROUP_TERMINAL_BLOCK_HEADER;

typedef struct
{
    UMP_GROUP_TERMINAL_BLOCK_HEADER GrpTrmBlock;
    WCHAR                           Name[1];             // NULL Terminated string, blank indicates none available
    // from USB Device
} UMP_GROUP_TERMINAL_BLOCK_DEFINITION, * PUMP_GROUP_TERMINAL_BLOCK_DEFINITION;
#pragma pack(pop)

_Use_decl_annotations_
HRESULT
CMidiDeviceManager::Initialize(
    std::shared_ptr<CMidiPerformanceManager>& performanceManager, 
    std::shared_ptr<CMidiEndpointProtocolManager>& endpointProtocolManager,
    std::shared_ptr<CMidiConfigurationManager>& configurationManager,
    std::shared_ptr<CMidiClientManager>& clientManager)
{
    DWORD transferState = 0;
    DWORD dataSize = sizeof(DWORD);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    m_configurationManager = configurationManager;

    // query for midisrv control keys
    if (ERROR_SUCCESS == RegGetValue(HKEY_LOCAL_MACHINE, driver32Path, midisrvTransferComplete, RRF_RT_DWORD, NULL, &transferState, &dataSize) && 
        transferState != 1)
    {
        // transferState will be 1 if Midi2 is enabled on the system and endpoint control has been transfered from
        // AudioEndpointBuilder to midisrv.
        //
        // If control has not been transferred, we do not want midisrv to create any midi 1 endpoints as they will
        // not be usable by the legacy api's.
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"AudioEndpointBuilder retains control of Midi port registration, unable to take exclusive control of midi ports.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        m_CreateMidi1Ports = false;
    }

    // Get the enabled transport layers from the registry
    for (auto const& TransportLayer : m_configurationManager->GetEnabledTransports())
    {
        wil::com_ptr_nothrow<IMidiTransport> midiTransport;
        wil::com_ptr_nothrow<IMidiEndpointManager> endpointManager;
        wil::com_ptr_nothrow<IMidiTransportConfigurationManager> transportConfigurationManager;

        try
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Getting transport configuration JSON", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingGuid(TransportLayer, "transport layer")
            );

            // provide the initial settings for these transports
            auto transportSettingsJson = m_configurationManager->GetSavedConfigurationForTransport(TransportLayer);

            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"CoCreating transport layer", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingGuid(TransportLayer, "transport layer")
            );

            // Do not load any transports which are untrusted, unless in developer mode.
            if (FAILED(internal::IsComponentPermitted(TransportLayer)))
            {
                continue;
            }

            // changed these from a return-on-fail to just log, so we don't prevent service startup
            // due to one bad transport

            LOG_IF_FAILED(CoCreateInstance(TransportLayer, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiTransport)));

            if (midiTransport != nullptr)
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Activating transport configuration manager.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingGuid(TransportLayer, "transport layer")
                );

                // we only log. This interface is optional
                // If present, an transport device manager may make use of its configuration manager, so activate
                // and initialize the configuration manager before activating the device manager so that configuration
                // is present.
                auto configHR = midiTransport->Activate(__uuidof(IMidiTransportConfigurationManager), (void**)&transportConfigurationManager);
                if (SUCCEEDED(configHR))
                {
                    if (transportConfigurationManager != nullptr)
                    {
                        // init the transport's config manager
                        auto initializeResult = transportConfigurationManager->Initialize(
                            TransportLayer,
                            this, 
                            m_configurationManager.get());

                        if (FAILED(initializeResult))
                        {
                            LOG_IF_FAILED(initializeResult);

                            TraceLoggingWrite(
                                MidiSrvTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Failed to initialize transport configuration manager.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                TraceLoggingHResult(initializeResult, MIDI_TRACE_EVENT_HRESULT_FIELD),
                                TraceLoggingGuid(TransportLayer, "transport layer")
                            );
                        }
                        else
                        {
                            // don't std::move this because we sill need it
                            m_midiTransportConfigurationManagers[TransportLayer] = transportConfigurationManager;

                            if (!transportSettingsJson.empty())
                            {
                                TraceLoggingWrite(
                                    MidiSrvTelemetryProvider::Provider(),
                                    MIDI_TRACE_EVENT_INFO,
                                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                    TraceLoggingPointer(this, "this"),
                                    TraceLoggingWideString(L"Updating transport configuration", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                    TraceLoggingGuid(TransportLayer, "transport layer")
                                );

                                LPWSTR response{};

                                auto updateConfigHR = transportConfigurationManager->UpdateConfiguration(transportSettingsJson.c_str(), &response);

                                if (FAILED(updateConfigHR))
                                {
                                    if (updateConfigHR == E_NOTIMPL)
                                    {
                                        TraceLoggingWrite(
                                            MidiSrvTelemetryProvider::Provider(),
                                            MIDI_TRACE_EVENT_WARNING,
                                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                                            TraceLoggingPointer(this, "this"),
                                            TraceLoggingWideString(L"Config update not implemented for this transport.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                            TraceLoggingHResult(updateConfigHR, MIDI_TRACE_EVENT_HRESULT_FIELD),
                                            TraceLoggingGuid(TransportLayer, "transport layer")
                                        );
                                    }
                                    else
                                    {
                                        LOG_IF_FAILED(updateConfigHR);

                                        TraceLoggingWrite(
                                            MidiSrvTelemetryProvider::Provider(),
                                            MIDI_TRACE_EVENT_ERROR,
                                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                            TraceLoggingPointer(this, "this"),
                                            TraceLoggingWideString(L"Failed to update configuration.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                            TraceLoggingHResult(updateConfigHR, MIDI_TRACE_EVENT_HRESULT_FIELD),
                                            TraceLoggingGuid(TransportLayer, "transport layer")
                                        );
                                    }
                                }
                                else
                                {

                                    TraceLoggingWrite(
                                        MidiSrvTelemetryProvider::Provider(),
                                        MIDI_TRACE_EVENT_INFO,
                                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                        TraceLoggingPointer(this, "this"),
                                        TraceLoggingWideString(L"Configuration updated.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                        TraceLoggingGuid(TransportLayer, "transport layer")
                                    );
                                }

                            }


                        }
                    }
                    else
                    {
                        TraceLoggingWrite(
                            MidiSrvTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_ERROR,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Transport transport configuration manager activation failed with null result.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                            TraceLoggingGuid(TransportLayer, "transport layer")
                        );
                    }
                }
                else
                {
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingHResult(configHR, MIDI_TRACE_EVENT_HRESULT_FIELD),
                        TraceLoggingWideString(L"Transport transport configuration manager activation failed.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingGuid(TransportLayer, "transport layer")
                    );
                }

                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Activating transport layer IMidiEndpointManager", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingGuid(TransportLayer, "transport layer")
                );

                LOG_IF_FAILED(midiTransport->Activate(__uuidof(IMidiEndpointManager), (void**)&endpointManager));

                if (endpointManager != nullptr)
                {
                    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> protocolManager = endpointProtocolManager.get();

                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Initializing transport layer Midi Endpoint Manager", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingGuid(TransportLayer, "transport layer")
                    );

                    auto initializeResult = endpointManager->Initialize(this, protocolManager.get());

                    if (SUCCEEDED(initializeResult))
                    {
                        m_midiEndpointManagers.emplace(TransportLayer, std::move(endpointManager));

                        TraceLoggingWrite(
                            MidiSrvTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_INFO,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Midi Endpoint Manager initialized", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                            TraceLoggingGuid(TransportLayer, "transport layer")
                        );
                    }
                    else
                    {
                        TraceLoggingWrite(
                            MidiSrvTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_ERROR,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Transport transport initialization failed.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                            TraceLoggingGuid(TransportLayer, "transport layer")
                        );
                    }

                }
                else
                {
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Transport transport endpoint manager initialization failed.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingGuid(TransportLayer, "transport layer")
                    );

                }
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Transport Transport activation failed (nullptr return).", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingGuid(TransportLayer, "transport layer")
                );
            }

        }

        catch (wil::ResultException rex)
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Result Exception loading transport transport.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingString(rex.what(), "error"),
                TraceLoggingHResult(rex.GetErrorCode(), MIDI_TRACE_EVENT_HRESULT_FIELD),
                TraceLoggingGuid(TransportLayer, "transport layer")
            );

        }
        catch (std::runtime_error& err)
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Runtime error loading transport transport.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingString(err.what(), "error"),
                TraceLoggingGuid(TransportLayer, "transport layer")
            );
        }
        catch (const std::exception& ex)
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Exception loading transport transport.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingString(ex.what(), "exception"),
                TraceLoggingGuid(TransportLayer, "transport layer")
            );
        }        
        catch (...)
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Unknown exception loading transport transport.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingGuid(TransportLayer, "transport layer")
            );

        }
    }

    m_performanceManager = performanceManager;
    m_clientManager = clientManager;

    return S_OK;
}

typedef struct _CREATECONTEXT
{
    PMIDIPORT MidiPort{ nullptr };
    wil::unique_event CreationCompleted{wil::EventOptions::None};
    DEVPROPERTY* InterfaceDevProperties{ nullptr };
    ULONG IntPropertyCount{ 0 };
} CREATECONTEXT, * PCREATECONTEXT;

typedef struct _PARENTDEVICECREATECONTEXT
{
    PMIDIPARENTDEVICE MidiParentDevice{ nullptr };
    wil::unique_event CreationCompleted{ wil::EventOptions::None };
} PARENTDEVICECREATECONTEXT, * PPARENTDEVICECREATECONTEXT;



VOID WINAPI
SwMidiPortCreateCallback(__in HSWDEVICE swDevice, __in HRESULT creationResult, __in PVOID context, __in_opt PCWSTR /* deviceInstanceId */)
{
    // fix code analysis complaint
    if (context == nullptr) return;

    PCREATECONTEXT creationContext = (PCREATECONTEXT)context;

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(creationContext->MidiPort->InstanceId.c_str(), "instance id"),
        TraceLoggingULong(creationContext->IntPropertyCount, "property count")
    );


    // interface registration has started, assume
    // failure
    creationContext->MidiPort->SwDeviceState = SWDEVICESTATE::Failed;
    creationContext->MidiPort->hr = creationResult;

    if (SUCCEEDED(creationContext->MidiPort->hr))
    {
        creationContext->MidiPort->hr = SwDeviceInterfaceRegister(
            swDevice,
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

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(nullptr, "this"),
            TraceLoggingWideString(L"Creation succeeded", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(creationContext->MidiPort->InstanceId.c_str(), "instance id"),
            TraceLoggingWideString(creationContext->MidiPort->DeviceInterfaceId.get(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );
    }

    // success or failure, signal we have completed.
    creationContext->CreationCompleted.SetEvent();
}


VOID WINAPI
SwMidiParentDeviceCreateCallback(
    __in HSWDEVICE /*swDevice*/,
    __in HRESULT creationResult,
    __in_opt PVOID context,
    __in_opt PCWSTR deviceInstanceId
)
{
    if (context == nullptr)
    {
        // TODO: Should log this.

        return;
    }

    PPARENTDEVICECREATECONTEXT creationContext = (PPARENTDEVICECREATECONTEXT)context;

    // interface registration has started, assume failure
    creationContext->MidiParentDevice->SwDeviceState = SWDEVICESTATE::Failed;
    creationContext->MidiParentDevice->hr = creationResult;

    LOG_IF_FAILED(creationResult);

    if (SUCCEEDED(creationResult))
    {
        // success, mark the port as created
        creationContext->MidiParentDevice->SwDeviceState = SWDEVICESTATE::Created;

        // get the new device instance ID. This is usually modified from what we started with
        creationContext->MidiParentDevice->InstanceId = std::wstring(deviceInstanceId);
    }

    // success or failure, signal we have completed.
    creationContext->CreationCompleted.SetEvent();
}


_Use_decl_annotations_
HRESULT
CMidiDeviceManager::DeactivateVirtualParentDevice(
    LPCWSTR instanceId)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(instanceId, MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD)
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, instanceId);

    auto cleanId = internal::NormalizeDeviceInstanceIdWStringCopy(instanceId);
    RETURN_HR_IF(E_INVALIDARG, cleanId == L"");

    std::vector<std::wstring> interfaceIds;
    bool parentRemoved{ false };

    do
    {
        // locate the MIDIPORT that identifies the swd
        // NOTE: This uses instanceId, not the Device Interface Id
        auto item = std::find_if(m_midiParents.begin(), m_midiParents.end(), [&](const std::unique_ptr<MIDIPARENTDEVICE>& parent)
            {
                if (internal::NormalizeDeviceInstanceIdWStringCopy(parent->InstanceId).starts_with(cleanId))
                {
                    return true;
                }

                return false;
            });

        // exit if the item was not found. We're done.
        if (item == m_midiParents.end())
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Found no more matches in list. Breaking out of loop", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(cleanId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD)
            );

            break;
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Found instance id in ports list. Erasing", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(cleanId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD),
                TraceLoggingWideString(item->get()->DeviceId.get(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

            // Add the interface ID of this port to a list
            interfaceIds.push_back(internal::NormalizeEndpointInterfaceIdWStringCopy(item->get()->DeviceId.get()));

            // Erasing this item from the list will free the unique_ptr and also trigger a SwDeviceClose on the item->SwDevice,
            // which will deactivate the device, done.
            m_midiParents.erase(item);
            parentRemoved = true;
        }
    } while (TRUE);


    // now, need to remove any children
    if (parentRemoved)
    {
        DeactivateEndpoint(instanceId);
    }

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(cleanId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD)
    );


    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiDeviceManager::ActivateVirtualParentDevice
(
    ULONG devicePropertyCount,
    const DEVPROPERTY* deviceProperties,
    const SW_DEVICE_CREATE_INFO* createInfo,
    LPWSTR* createdSwDeviceId
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    RETURN_HR_IF_NULL(E_INVALIDARG, createInfo);
    RETURN_HR_IF_NULL(E_INVALIDARG, createdSwDeviceId);

    auto pcreateinfo = (SW_DEVICE_CREATE_INFO *)createInfo;

    std::unique_ptr<MIDIPARENTDEVICE> midiParent = std::make_unique<MIDIPARENTDEVICE>();
    RETURN_IF_NULL_ALLOC(midiParent);

    midiParent->SwDeviceState = SWDEVICESTATE::CreatePending;

    PARENTDEVICECREATECONTEXT creationContext;
    creationContext.MidiParentDevice = midiParent.get();

    std::vector<DEVPROPERTY> devProperties{};

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;

    if (devicePropertyCount > 0 && deviceProperties != nullptr)
    {
        for (ULONG i = 0; i < devicePropertyCount; i++)
        {
            devProperties.push_back(((DEVPROPERTY*)deviceProperties)[i]);
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
    if (FALSE == creationContext.CreationCompleted.wait(SWD_CREATION_TIMEOUT))
    {
        // Activation of the SWD has failed, resetting the SwDevice handle
        // may unblock the creation callback, letting it complete (with failure),
        // give it an opportunity to complete.
        // If the callback were to run after this function exits, 
        // the service will crash because the creation context given to
        // the callback will no longer exist.
        midiParent->SwDevice.reset();
        creationContext.CreationCompleted.wait(SWD_CREATION_TIMEOUT);
        RETURN_IF_FAILED(midiParent->hr);
        RETURN_IF_FAILED(E_FAIL);
    }

    // confirm we were able to register the interface
    RETURN_IF_FAILED(creationContext.MidiParentDevice->hr);
    RETURN_HR_IF(E_FAIL, creationContext.MidiParentDevice->SwDeviceState != SWDEVICESTATE::Created);

    wil::unique_cotaskmem_string newDeviceId;
    newDeviceId = wil::make_cotaskmem_string_nothrow(creationContext.MidiParentDevice->InstanceId.c_str());
    RETURN_IF_NULL_ALLOC(newDeviceId.get());
    *createdSwDeviceId = newDeviceId.release();

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(*createdSwDeviceId)
    );

    if (SUCCEEDED(creationContext.MidiParentDevice->hr))
    {
        m_midiParents.push_back(std::move(midiParent));
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiDeviceManager::ActivateEndpoint
(
    PCWSTR parentInstanceId,
    BOOL umpOnly,
    MidiFlow flow,
    const PMIDIENDPOINTCOMMONPROPERTIES commonProperties,
    ULONG intPropertyCount,
    ULONG devPropertyCount,
    const DEVPROPERTY* interfaceDevProperties,
    const DEVPROPERTY* deviceDevProperties,
    const SW_DEVICE_CREATE_INFO* createInfo,
    LPWSTR *createdDeviceInterfaceId
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(parentInstanceId, "parent"),
        TraceLoggingBool(commonProperties != nullptr, "Common Properties Provided"),
        TraceLoggingULong(intPropertyCount, "Interface Property Count"),
        TraceLoggingULong(devPropertyCount, "Device Property Count"),
        TraceLoggingBool(umpOnly, "UMP Only")
    );

    auto lock = m_midiPortsLock.lock();

    //OutputDebugString(L"Activate: ");
    //OutputDebugString(((SW_DEVICE_CREATE_INFO*)createInfo)->pszInstanceId);
    //OutputDebugString(L"\n");


    const bool alreadyActivated = std::find_if(m_midiPorts.begin(), m_midiPorts.end(), [&](const std::unique_ptr<MIDIPORT>& Port)
    {
        //OutputDebugString(L"Activate Checking: ");
        //OutputDebugString(Port->InstanceId.c_str());
        //OutputDebugString(L"\n");

        // if this instance id already activated, then we cannot activate/create a second time,
        if (((SW_DEVICE_CREATE_INFO*)createInfo)->pszInstanceId == Port->InstanceId)
        {
            return true;
        }

        return false;
    }) != m_midiPorts.end();

    if (alreadyActivated)
    {
        //OutputDebugString(L"Already Activated: ");
        //OutputDebugString(((SW_DEVICE_CREATE_INFO*)createInfo)->pszInstanceId);
        //OutputDebugString(L"\n");

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Already Activated", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(parentInstanceId, "parent"),
            TraceLoggingWideString(((SW_DEVICE_CREATE_INFO*)createInfo)->pszInstanceId, "port instance")
            );

        return S_FALSE;
    }
    else
    {
        //OutputDebugString(L"NOT Already Activated: ");
        //OutputDebugString(((SW_DEVICE_CREATE_INFO*)createInfo)->pszInstanceId);
        //OutputDebugString(L"\n");

        std::vector<DEVPROPERTY> allInterfaceProperties{};

        if (intPropertyCount > 0 && interfaceDevProperties != nullptr)
        {
            // copy the incoming array into a vector so that we can add additional items.
            std::vector<DEVPROPERTY> suppliedInterfaceProperties((DEVPROPERTY*)interfaceDevProperties, (DEVPROPERTY*)(interfaceDevProperties) + intPropertyCount);

            // copy to the main vector that we're going to keep using.
            allInterfaceProperties = suppliedInterfaceProperties;


            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(parentInstanceId, "parent"),
                TraceLoggingULong((ULONG)intPropertyCount, "provided interface props count"),
                TraceLoggingULong((ULONG)allInterfaceProperties.size(), "copied interface props count")
            );
        }

        DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
        DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;

        // Build common properties so these are consistent from endpoint to endpoint
        if (commonProperties != nullptr)
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(parentInstanceId, "parent"),
                TraceLoggingWideString(L"Common properties object supplied", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );


            allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_TransportLayer, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_GUID, (ULONG)(sizeof(GUID)), (PVOID)(&(commonProperties->TransportId)) });

            allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointDevicePurpose, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_UINT32, (ULONG)(sizeof(UINT32)), (PVOID)(&(commonProperties->EndpointDeviceType)) });
            
            if (commonProperties->FriendlyName != nullptr && wcslen(commonProperties->FriendlyName) > 0)
            {
                allInterfaceProperties.push_back(DEVPROPERTY{ {DEVPKEY_DeviceInterface_FriendlyName, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (wcslen(commonProperties->FriendlyName) + 1)), (PVOID)(commonProperties->FriendlyName) });
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Common Properties: Friendly Name is null or empty", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(parentInstanceId, "parent")
                    );

                return E_INVALIDARG;
            }

            // transport information

            if (commonProperties->TransportCode != nullptr && wcslen(commonProperties->TransportCode) > 0)
            {
                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_TransportCode, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (wcslen(commonProperties->TransportCode) + 1)), (PVOID)commonProperties->TransportCode });
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(parentInstanceId),
                    TraceLoggingWideString(L"Common Properties: Transport Code is null or empty", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );

                return E_INVALIDARG;
            }


            if (commonProperties->EndpointName != nullptr && wcslen(commonProperties->EndpointName) > 0)
            {
                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointName, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (wcslen(commonProperties->EndpointName) + 1)), (PVOID)commonProperties->EndpointName });
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(parentInstanceId),
                    TraceLoggingWideString(L"Clearing PKEY_MIDI_EndpointName", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );
                
                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointName, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
            }

            if (commonProperties->EndpointDescription != nullptr && wcslen(commonProperties->EndpointDescription) > 0)
            {
                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_Description, DEVPROP_STORE_SYSTEM, nullptr},
                   DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (wcslen(commonProperties->EndpointDescription) + 1)), (PVOID)commonProperties->EndpointDescription });
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(parentInstanceId),
                    TraceLoggingWideString(L"Clearing PKEY_MIDI_Description", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );

                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_Description, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
            }

            if (commonProperties->UniqueIdentifier != nullptr && wcslen(commonProperties->UniqueIdentifier) > 0)
            {
                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_SerialNumber, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (wcslen(commonProperties->UniqueIdentifier) + 1)), (PVOID)commonProperties->UniqueIdentifier });
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(parentInstanceId),
                    TraceLoggingWideString(L"Clearing PKEY_MIDI_SerialNumber", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );

                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_SerialNumber, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
            }

            if (commonProperties->ManufacturerName != nullptr && wcslen(commonProperties->ManufacturerName) != 0)
            {
                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_ManufacturerName, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (wcslen(commonProperties->ManufacturerName) + 1)), (PVOID)commonProperties->ManufacturerName });
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(parentInstanceId),
                    TraceLoggingWideString(L"Clearing PKEY_MIDI_ManufacturerName", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );

                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_ManufacturerName, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
            }

            // user-supplied information, if available

            if (commonProperties->CustomEndpointName != nullptr && wcslen(commonProperties->CustomEndpointName) != 0)
            {
                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_CustomEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (wcslen(commonProperties->CustomEndpointName) + 1)), (PVOID)commonProperties->CustomEndpointName });
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(parentInstanceId),
                    TraceLoggingWideString(L"Clearing PKEY_MIDI_CustomEndpointName", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );

                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_CustomEndpointName, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
            }

            if (commonProperties->CustomEndpointDescription != nullptr && wcslen(commonProperties->CustomEndpointDescription) != 0)
            {
                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_CustomDescription, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (wcslen(commonProperties->CustomEndpointDescription) + 1)), (PVOID)commonProperties->CustomEndpointDescription });
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(parentInstanceId),
                    TraceLoggingWideString(L"Clearing PKEY_MIDI_CustomDescription", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );

                allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_CustomDescription, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
            }

            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(parentInstanceId, "parent"),
                TraceLoggingWideString(L"Adding other non-string properties", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            // data format. These each have different uses. Native format is the device's format. Supported
            // format is how we talk to it from the service (the driver may translate to UMP, for example)

            allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_NativeDataFormat, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BYTE, (ULONG)(sizeof(BYTE)), (PVOID)(&(commonProperties->NativeDataFormat)) });

            allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_SupportedDataFormats, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BYTE, (ULONG)(sizeof(BYTE)), (PVOID)(&(commonProperties->SupportedDataFormats)) });

            // Protocol defaults for cases when protocol negotiation may not happen or may not complete
            allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointSupportsMidi1Protocol, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BOOLEAN, (ULONG)(sizeof(DEVPROP_BOOLEAN)), (PVOID)((commonProperties->Capabilities & MidiEndpointCapabilities_SupportsMidi1Protocol)? &devPropTrue : &devPropFalse) });

            allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointSupportsMidi2Protocol, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BOOLEAN, (ULONG)(sizeof(DEVPROP_BOOLEAN)), (PVOID)((commonProperties->Capabilities & MidiEndpointCapabilities_SupportsMidi2Protocol) ? &devPropTrue : &devPropFalse) });

            // behavior

            allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_GenerateIncomingTimestamp, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BOOLEAN, (ULONG)(sizeof(DEVPROP_BOOLEAN)), (PVOID)((commonProperties->Capabilities & MidiEndpointCapabilities_GenerateIncomingTimestamps) ? &devPropTrue : &devPropFalse) });

            allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_SupportsMulticlient, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BOOLEAN, (ULONG)(sizeof(DEVPROP_BOOLEAN)), (PVOID)((commonProperties->Capabilities & MidiEndpointCapabilities_SupportsMultiClient) ? &devPropTrue : &devPropFalse) });
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(parentInstanceId, "parent"),
                TraceLoggingWideString(L"NO Common properties object supplied", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

        }

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(parentInstanceId, "parent"),
            TraceLoggingWideString(L"Adding clearing properties", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        // Clear almost all of the protocol negotiation-discovered properties. We keep the "supports MIDI 1.0" and "supports MIDI 2.0" 
        // defaulted based on the native transport type of the device as per above.
       
        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_EndpointDiscoveryProcessComplete, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });

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
        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_FunctionBlockDeclaredCount, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
        allInterfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_FunctionBlocksLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });

        // remove actual function blocks and their names
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Adding clearing properties for function blocks", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(parentInstanceId, "parent")
        );

        for (int i = 0; i < MIDI_MAX_FUNCTION_BLOCKS; i++)
        {
            // clear out the function block data
            DEVPROPKEY fbkey;
            fbkey.fmtid = PKEY_MIDI_FunctionBlock00.fmtid;    // hack
            fbkey.pid = MIDI_FUNCTION_BLOCK_PROPERTY_INDEX_START + i;

            allInterfaceProperties.push_back(DEVPROPERTY{ {fbkey, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });

            // clear out the function block name data
            DEVPROPKEY namekey;
            namekey.fmtid = PKEY_MIDI_FunctionBlockName00.fmtid;    // hack
            namekey.pid = MIDI_FUNCTION_BLOCK_NAME_PROPERTY_INDEX_START + i;

            allInterfaceProperties.push_back(DEVPROPERTY{ {namekey, DEVPROP_STORE_SYSTEM, nullptr}, DEVPROP_TYPE_EMPTY, 0, nullptr });
        }

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Registering cleanupOnFailure", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(parentInstanceId, "parent")
            );

        std::wstring deviceInterfaceId{ };
        PMIDIPORT createdMidiPort {};

        auto cleanupOnFailure = wil::scope_exit([&]()
        {
            // in the event that creation fails, clean up all of the interfaces associated with this InstanceId
            DeactivateEndpoint(((SW_DEVICE_CREATE_INFO*)createInfo)->pszInstanceId);
        });


        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Activating UMP Endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(parentInstanceId, "parent")
        );
        
        // Activate the UMP version of this endpoint.
        auto activationResult = ActivateEndpointInternal(
            parentInstanceId, 
            nullptr, 
            FALSE,
            0,
            flow, 
            (ULONG)allInterfaceProperties.size(),
            devPropertyCount, 
            (DEVPROPERTY*)(allInterfaceProperties.data()),
            (DEVPROPERTY*)deviceDevProperties, 
            (SW_DEVICE_CREATE_INFO*)createInfo, 
            &deviceInterfaceId,
            &createdMidiPort);

        if (FAILED(activationResult))
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Failed to activate UMP endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(parentInstanceId, "parent"),
                TraceLoggingHResult(activationResult, "Activation HRESULT")
            );

            RETURN_IF_FAILED(activationResult);
        }

        // track if this port should only have UMP endpoints
        createdMidiPort->UmpOnly = umpOnly;

        if (!umpOnly)
        {
            LOG_IF_FAILED(SyncMidi1Ports(createdMidiPort));
        }

        // return the created device interface Id. This is needed for anything that will 
        // do a match in the Ids from Windows.Devices.Enumeration
        if (createdDeviceInterfaceId != nullptr)
        {
            wil::unique_cotaskmem_string tempString = wil::make_cotaskmem_string_nothrow(deviceInterfaceId.c_str());
            RETURN_IF_NULL_ALLOC(tempString.get());
            *createdDeviceInterfaceId = tempString.release();
        }

        cleanupOnFailure.release();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiDeviceManager::ActivateEndpointInternal
(
    PCWSTR parentInstanceId,
    PCWSTR associatedInterfaceId,
    BOOL midiOne,
    UINT32 groupIndex,
    MidiFlow flow,
    ULONG intPropertyCount,
    ULONG devPropertyCount,
    const DEVPROPERTY *interfaceDevProperties,
    const DEVPROPERTY *deviceDevProperties,
    const SW_DEVICE_CREATE_INFO *createInfo,
    std::wstring *deviceInterfaceId,
    PMIDIPORT* createdPort
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, interfaceDevProperties);
    RETURN_HR_IF(E_INVALIDARG, intPropertyCount == 0);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(parentInstanceId, "parent instance id"),
        TraceLoggingWideString(associatedInterfaceId, "associated interface id"),
        TraceLoggingBool(midiOne, "midi 1"),
        TraceLoggingULong(intPropertyCount, "interface prop count"),
        TraceLoggingULong(devPropertyCount, "device prop count")
    );

    SW_DEVICE_CREATE_INFO internalCreateInfo = *createInfo;

    std::unique_ptr<MIDIPORT> midiPort = std::make_unique<MIDIPORT>();
    RETURN_IF_NULL_ALLOC(midiPort);

    std::vector<DEVPROPERTY> interfaceProperties{};

    // copy the incoming array into a vector so that we can add additional items.
    if (interfaceDevProperties != nullptr && intPropertyCount > 0)
    {
        std::vector<DEVPROPERTY> existingInterfaceProperties(interfaceDevProperties, interfaceDevProperties + intPropertyCount);

        // copy 'em over
        interfaceProperties = existingInterfaceProperties;
    }

    if (associatedInterfaceId != nullptr && wcslen(associatedInterfaceId) > 0)
    {
        // add any additional required items to the vector
        interfaceProperties.push_back({ {PKEY_MIDI_AssociatedUMP, DEVPROP_STORE_SYSTEM, nullptr},
                   DEVPROP_TYPE_STRING, static_cast<ULONG>((wcslen(associatedInterfaceId) + 1) * sizeof(WCHAR)), (PVOID)associatedInterfaceId });

        midiPort->AssociatedInterfaceId = associatedInterfaceId;
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

    midiPort->InstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(createInfo->pszInstanceId);
    midiPort->Flow = flow;
    midiPort->Enumerator = midiOne ? AUDIO_DEVICE_ENUMERATOR : MIDI_DEVICE_ENUMERATOR;
    midiPort->MidiOne = midiOne;
    midiPort->ParentInstanceId = parentInstanceId;
    midiPort->DeviceDescription = createInfo->pszDeviceDescription;
    if (nullptr != createInfo->pContainerId)
    {
        midiPort->ContainerId = *(createInfo->pContainerId);
    }

    internalCreateInfo.pszzCompatibleIds = nullptr;
    internalCreateInfo.pszzHardwareIds = nullptr;

    if (midiOne)
    {
        midiPort->GroupIndex = groupIndex;

        // append the group index to the instance id, so we an get a separate SWD for
        // each group, otherwise we'll only get 1 SWD per flow.
        midiPort->InstanceId += L"_";
        midiPort->InstanceId += std::to_wstring(flow);
        midiPort->InstanceId += L"_";
        midiPort->InstanceId += std::to_wstring(groupIndex);

        interfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_PortAssignedGroupIndex, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_UINT32, (ULONG)(sizeof(UINT32)), (PVOID)(&groupIndex) });

        if (flow == MidiFlowOut)
        {
            midiPort->InterfaceCategory = &DEVINTERFACE_MIDI_OUTPUT;
        }
        else if (flow == MidiFlowIn)
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
        internalCreateInfo.pszzCompatibleIds = szzMidiDeviceCompatibleId;
        internalCreateInfo.pszzHardwareIds = szzMidiDeviceHardwareId;

        if (flow == MidiFlowOut)
        {
            midiPort->InterfaceCategory = &DEVINTERFACE_UNIVERSALMIDIPACKET_OUTPUT;
        }
        else if (flow == MidiFlowIn)
        {
            midiPort->InterfaceCategory = &DEVINTERFACE_UNIVERSALMIDIPACKET_INPUT;
        }
        else if (flow == MidiFlowBidirectional)
        {
            midiPort->InterfaceCategory = &DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI;
        }
        else
        {
            RETURN_IF_FAILED(E_INVALIDARG);
        }
    }

    // create using the normalized, and possibly updated, instance id that is on the midiPort.
    internalCreateInfo.pszInstanceId = midiPort->InstanceId.c_str();

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
        parentInstanceId,
        &internalCreateInfo,
        devPropertyCount,
        devPropertyCount == 0 ? (DEVPROPERTY*)nullptr : deviceDevProperties,
        SwMidiPortCreateCallback,
        &creationContext,
        wil::out_param(midiPort->SwDevice));

    if (SUCCEEDED(midiPort->hr))
    {
        // wait 10 seconds for creation to complete
        if (FALSE == creationContext.CreationCompleted.wait(SWD_CREATION_TIMEOUT))
        {
            // Activation of the SWD has failed, resetting the SwDevice handle
            // may unblock the creation callback, letting it complete (with failure),
            // give it an opportunity to complete.
            // If the callback were to run after this function exits, 
            // the service will crash because the creation context given to
            // the callback will no longer exist.
            midiPort->SwDevice.reset();
            creationContext.CreationCompleted.wait(SWD_CREATION_TIMEOUT);
            RETURN_IF_FAILED(midiPort->hr);
            RETURN_IF_FAILED(E_FAIL);
        }
    }
    else if (HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS) == midiPort->hr)
    {
        //TraceLoggingWrite(
        //    MidiSrvTelemetryProvider::Provider(),
        //    MIDI_TRACE_EVENT_INFO,
        //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        //    TraceLoggingPointer(this, "this"),
        //    TraceLoggingWideString(L"Endpoint already exists."),
        //    TraceLoggingWideString(parentInstanceId, "parent"),
        //    TraceLoggingBool(midiOne)
        //);

        // if the devnode already exists, then we likely only need to create/activate the interface, a previous
        // call created the devnode. First, locate the matching SwDevice handle for the existing devnode.
        auto item = std::find_if(m_midiPorts.begin(), m_midiPorts.end(), [&](const std::unique_ptr<MIDIPORT>& Port)
        {
            // if this instance id already activated, then we cannot activate/create a second time,
            if (midiPort->InstanceId == Port->InstanceId &&
                midiPort->Enumerator == Port->Enumerator)
            {
                return true;
            }

            return false;
        });

        if (item != m_midiPorts.end())
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Activating previously created endpoint."),
                TraceLoggingWideString(parentInstanceId, "parent"),
                TraceLoggingWideString(midiPort->DeviceInterfaceId.get(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                TraceLoggingWideString(midiPort->InstanceId.c_str(), "new port"),
                TraceLoggingWideString(midiPort->Enumerator.c_str(), "new port enumerator"),
                TraceLoggingBool(midiOne)
            );

            midiPort->SwDevice = item->get()->SwDevice;
            SwMidiPortCreateCallback(item->get()->SwDevice.get(), S_OK, &creationContext, nullptr);
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Endpoint exists, but it is not in the MidiDeviceManager store. Is something else creating endpoints?", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(parentInstanceId, "parent"),
                TraceLoggingWideString(midiPort->DeviceInterfaceId.get(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                TraceLoggingWideString(midiPort->InstanceId.c_str(), "new port instance id"),
                TraceLoggingWideString(midiPort->Enumerator.c_str(), "new port enumerator"),
                TraceLoggingBool(midiOne)
            );
        }
    }
    else
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"SwDeviceCreate failed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(parentInstanceId, "parent"),
            TraceLoggingBool(midiOne)
        );
    }

    // confirm we were able to register the interface

    if (FAILED(midiPort->hr))
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"SwDeviceCreate returned a failed HR", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingHResult(midiPort->hr, MIDI_TRACE_EVENT_HRESULT_FIELD),
            TraceLoggingWideString(parentInstanceId, "parent"),
            TraceLoggingBool(midiOne)
        );
    }
    RETURN_IF_FAILED(midiPort->hr);

    if (midiPort->SwDeviceState != SWDEVICESTATE::Created)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"SwDeviceState != SWDEVICESTATE::Created", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(parentInstanceId, "parent"),
            TraceLoggingBool(midiOne)
        );
    }
    RETURN_HR_IF(E_FAIL, midiPort->SwDeviceState != SWDEVICESTATE::Created);

    if (midiOne)
    {
        // Assign the port number for this midi port
        RETURN_IF_FAILED(AssignPortNumber(midiPort->SwDevice.get(), midiPort->DeviceInterfaceId.get(), flow));
    }

    // Activate the SWD just created, it's created in the disabled state to allow for assigning
    // the port prior to activating it.
    RETURN_IF_FAILED(SwDeviceInterfaceSetState(
        midiPort->SwDevice.get(),
        midiPort->DeviceInterfaceId.get(),
        TRUE));

    if (deviceInterfaceId)
    {
        *deviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(midiPort->DeviceInterfaceId.get());

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(deviceInterfaceId->c_str(), "created device interface id")
        );
    }

    // success, transfer the midiPort to the list
    // and provide a copy of it to the caller if requested
    if (createdPort)
    {
        *createdPort = midiPort.get();
    }
    m_midiPorts.push_back(std::move(midiPort));

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiDeviceManager::UpdateEndpointProperties
(
    PCWSTR deviceInterfaceId,
    ULONG intPropertyCount,
    const DEVPROPERTY* interfaceDevProperties
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, deviceInterfaceId);
    RETURN_HR_IF_NULL(E_INVALIDARG, interfaceDevProperties);
    RETURN_HR_IF(E_INVALIDARG, intPropertyCount == 0);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(deviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingULong(intPropertyCount, "property count")
    );

    auto requestedInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(deviceInterfaceId);

    // Can't have the available midi ports changing while updating this, 
    // take the midi ports lock.
    auto lock = m_midiPortsLock.lock();

    // locate the MIDIPORT 
    auto item = std::find_if(m_midiPorts.begin(), m_midiPorts.end(), [&](const std::unique_ptr<MIDIPORT>& Port)
        {
            auto portInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(Port->DeviceInterfaceId.get());

            return (portInterfaceId == requestedInterfaceId);
        });

    if (item == m_midiPorts.end())
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Device not found. If the updates are from the config file, the device may not yet have been enumerated.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(requestedInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
            TraceLoggingULong(intPropertyCount, "property count")
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
            intPropertyCount,
            (const DEVPROPERTY*)interfaceDevProperties
        );

        if (SUCCEEDED(propSetHR))
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Properties set", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(requestedInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                TraceLoggingULong(intPropertyCount, "property count")
            );

            bool found {false};

            typedef struct _PROPERTY_COMPARE
            {
                DEVPROPKEY Key;
                bool       IsRange;
                DEVPROPID  MinPID;
                DEVPROPID  MaxPID;
            } PROPERTY_COMPARE, *PPROPERTY_COMPARE;

            PROPERTY_COMPARE interestingKeys[] = {
                { PKEY_MIDI_CustomPortNumber, false, 0, 0 },
                { PKEY_MIDI_CustomEndpointName, false, 0, 0 },
                { PKEY_MIDI_GroupTerminalBlocks, false, 0, 0 },
                { PKEY_MIDI_FunctionBlockDeclaredCount, false, 0, 0 },
                { PKEY_MIDI_FunctionBlocksAreStatic, false, 0, 0 },
                { PKEY_MIDI_FunctionBlocksLastUpdateTime, false, 0, 0 },
                { PKEY_MIDI_FunctionBlock00, true, PKEY_MIDI_FunctionBlock00.pid, PKEY_MIDI_FunctionBlock31.pid },
                { PKEY_MIDI_FunctionBlockName00, true, PKEY_MIDI_FunctionBlockName00.pid, PKEY_MIDI_FunctionBlockName31.pid },
                { PKEY_MIDI_EndpointDiscoveryProcessComplete, false, 0, 0 },
                { PKEY_MIDI_Midi1PortNameTable, false, 0, 0 },
                { PKEY_MIDI_Midi1PortNamingSelection, false, 0, 0 },
            };

            for (UINT i = 0; i < intPropertyCount && !found; i++)
            {
                for (UINT j = 0; j < _countof(interestingKeys) && !found; j++)
                {
                    // if the fmtid matches, it might be a match, need to check pid.
                    if (interfaceDevProperties[i].CompKey.Key.fmtid == interestingKeys[j].Key.fmtid)
                    {
                        // if the pid matches, or a pid range is specified and this pid is in the range,
                        // then it's a match.
                        if(interfaceDevProperties[i].CompKey.Key.pid == interestingKeys[j].Key.pid ||
                            (interestingKeys[j].IsRange && 
                                interfaceDevProperties[i].CompKey.Key.pid >= interestingKeys[j].MinPID &&
                                interfaceDevProperties[i].CompKey.Key.pid <= interestingKeys[j].MaxPID))
                        {
                            found = true;
                        }
                    }
                }
            }

            if (found)
            {
                // log rather than return because MIDI 1 port sync is more a side effect
                LOG_IF_FAILED(SyncMidi1Ports(item->get()));
            }

            return S_OK;
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Error setting properties", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingHResult(propSetHR, MIDI_TRACE_EVENT_HRESULT_FIELD),
                TraceLoggingWideString(requestedInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                TraceLoggingULong(intPropertyCount, "property count")
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
    PCWSTR deviceInterfaceId,
    ULONG intPropertyCount,
    const DEVPROPERTY* interfaceDevPropKeys
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(deviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingULong(intPropertyCount, "property count")
    );

    // create list of dev properties from the keys

    // Call UpdateEndpointProperties

    auto keys = (const DEVPROPKEY*)interfaceDevPropKeys;

    std::vector<DEVPROPERTY> properties;

    for (ULONG i = 0; i < intPropertyCount; i++)
    {
        properties.push_back({{ keys[i], DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_EMPTY, 0, NULL });
    }

    return UpdateEndpointProperties(deviceInterfaceId, (ULONG)properties.size(), properties.data());
}

_Use_decl_annotations_
HRESULT
CMidiDeviceManager::DeactivateEndpoint
(
    PCWSTR instanceId
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(instanceId, MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD)
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, instanceId);

    bool portsRemoved{ false };

    auto cleanId = internal::NormalizeDeviceInstanceIdWStringCopy(instanceId);
    RETURN_HR_IF(E_INVALIDARG, cleanId == L"");

    std::vector<std::wstring> interfaceIds{};

    // there may be more than one SWD associated with this instance id, as we reuse
    // the instance id for the legacy SWD, it just has a different activator and InterfaceClass.

    if (Feature_Servicing_MIDI2DeviceRemoval::IsEnabled())
    {
        do
        {
            auto lock = m_midiPortsLock.lock();

            // locate the MIDIPORT that identifies the swd
            // NOTE: This uses instanceId, not the Device Interface Id
            auto item = std::find_if(m_midiPorts.begin(), m_midiPorts.end(), [&](const std::unique_ptr<MIDIPORT>& Port)
            {
                // for MIDI 1 child ports for a device, the instance id can have a _n added where n is the group number
                // The transports only know about the UMP endpoint they created, therefore, we need to do a 
                // "starts with", not an exact match. 

                if (internal::NormalizeDeviceInstanceIdWStringCopy(Port->InstanceId).starts_with(cleanId) ||
                    internal::NormalizeDeviceInstanceIdWStringCopy(Port->ParentInstanceId).starts_with(cleanId))
                {
                    return true;
                }

                return false;
            });

            // exit if the item was not found. We're done.
            if (item == m_midiPorts.end())
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Found no more matches in list. Breaking out of loop", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(cleanId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD)
                );

                break;
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Found instance id in ports list. Erasing", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(cleanId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD),
                    TraceLoggingWideString(item->get()->DeviceInterfaceId.get(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                );

                // Add the interface ID of this port to a list
                interfaceIds.push_back(internal::NormalizeEndpointInterfaceIdWStringCopy(item->get()->DeviceInterfaceId.get()));

                // Erasing this item from the list will free the unique_ptr and also trigger a SwDeviceClose on the item->SwDevice,
                // which will deactivate the device, done.
                m_midiPorts.erase(item);
                portsRemoved = true;
            }
        } while (TRUE);
    }
    else
    {
        do
        {
            // locate the MIDIPORT that identifies the swd
            // NOTE: This uses instanceId, not the Device Interface Id
            auto item = std::find_if(m_midiPorts.begin(), m_midiPorts.end(), [&](const std::unique_ptr<MIDIPORT>& Port)
            {
                // for MIDI 1 child ports for a device, the instance id can have a _n added where n is the group number
                // The transports only know about the UMP endpoint they created, therefore, we need to do a 
                // "starts with", not an exact match. 
        
                if (internal::NormalizeDeviceInstanceIdWStringCopy(Port->InstanceId).starts_with(cleanId) ||
                    internal::NormalizeDeviceInstanceIdWStringCopy(Port->ParentInstanceId).starts_with(cleanId))
                {
                    return true;
                }
        
                return false;
            });
        
            // exit if the item was not found. We're done.
            if (item == m_midiPorts.end())
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Found no more matches in list. Breaking out of loop", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(cleanId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD)
                );
        
                break;
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Found instance id in ports list. Erasing", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(cleanId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD),
                    TraceLoggingWideString(item->get()->DeviceInterfaceId.get(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                );
        
                // Add the interface ID of this port to a list
                interfaceIds.push_back(internal::NormalizeEndpointInterfaceIdWStringCopy(item->get()->DeviceInterfaceId.get()));
        
                // Erasing this item from the list will free the unique_ptr and also trigger a SwDeviceClose on the item->SwDevice,
                // which will deactivate the device, done.
                m_midiPorts.erase(item);
                portsRemoved = true;
            }
        } while (TRUE);
    }


    if (portsRemoved)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Notifying client manager of device removal", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingValue(interfaceIds.size(), "InterfaceIdCount"),
            TraceLoggingWideString(interfaceIds.empty() ? L"<none>" : interfaceIds.front().c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
         );
        
        // Notify client manager that device was removed
        if (m_clientManager)
        {
            m_clientManager->OnDeviceRemoved(interfaceIds);
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"No client manager set for device removal", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );
        }

        // Potential race condition here?
        // now that we've removed UMP and WinMM ports, we need to compact the port numbers to keep them contiguous
        RETURN_IF_FAILED(CompactPortNumbers());
    }


    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(cleanId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD)
    );

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiDeviceManager::RemoveEndpoint
(
    PCWSTR instanceId 
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(instanceId, MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD)
    );

    // deactivate, to ensure it's removed from the tracking list
    // we are not deleting because it is beneficial to retain the SWD for caching.
    // If the parent device is removed, all the child SWD's will also be deleted,
    // so explicit removal here is not needed.
    LOG_IF_FAILED(DeactivateEndpoint(instanceId));

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiDeviceManager::UpdateTransportConfiguration
(
    GUID transportId,
    LPCWSTR configurationJson,
    LPWSTR* response
)
{
    if (configurationJson != nullptr)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingGuid(transportId, "transport id"),
            TraceLoggingWideString(configurationJson, "config json")
        );

        if (auto search = m_midiTransportConfigurationManagers.find(transportId); search != m_midiTransportConfigurationManagers.end())
        {
            auto configManager = search->second;

            if (configManager)
            {
                return configManager->UpdateConfiguration(configurationJson, response);
            }
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Failed to find the referenced transport by its GUID", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingGuid(transportId, "transport id")
            );

        }
    }
    else
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Json is null", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
    }


    // failed to find the transport or its related endpoint manager
    return E_FAIL;
}


HRESULT
CMidiDeviceManager::Shutdown()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    for (auto const& endpointManager : m_midiEndpointManagers)
    {
        LOG_IF_FAILED(endpointManager.second->Shutdown());
    }

    for (auto const& configurationManager : m_midiTransportConfigurationManagers)
    {
        LOG_IF_FAILED(configurationManager.second->Shutdown());
    }

    m_midiEndpointManagers.clear();
    m_midiTransportConfigurationManagers.clear();

    m_midiPorts.clear();

    m_midiParents.clear();

    return S_OK;
}

#define MIDI1_OUTPUT_DEVICES \
    L"System.Devices.InterfaceClassGuid:=\"{6DC23320-AB33-4CE4-80D4-BBB3EBBF2814}\""

#define MIDI1_INPUT_DEVICES \
    L"System.Devices.InterfaceClassGuid:=\"{504BE32C-CCF6-4D2C-B73F-6F8B3747E22B}\""


HRESULT
CMidiDeviceManager::CompactPortNumbers()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // we don't want any ports added while we're doing this
    auto lock = m_midiPortsLock.lock();

    for (auto flow : { MidiFlowOut, MidiFlowIn })
    {
        std::map<UINT32, std::wstring> ports;

        // for this version, we only check active devices. We're not trying to preserve or reserve numbers
        winrt::hstring deviceSelector(flow == MidiFlowOut ? MIDI1_OUTPUT_DEVICES : MIDI1_INPUT_DEVICES);
        deviceSelector = deviceSelector + L" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";

        auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
        additionalProperties.Append(STRING_PKEY_MIDI_ServiceAssignedPortNumber);

        auto deviceList = DeviceInformation::FindAllAsync(deviceSelector, additionalProperties).get();

        UINT32 highestOldPortNumber{ 1 };   // our internal numbers start at 1. For midi inputs, we subtract 1 in WinMM code. For MIDI out, GS becomes 0.

        // get all the current port numbers
        for (auto const& device : deviceList)
        {
            auto servicePortNum = internal::SafeGetSwdPropertyFromDeviceInformation<UINT32>(STRING_PKEY_MIDI_ServiceAssignedPortNumber, device, 0);

            // 0 for this property is not valid. They start at 1 for the prop value
            if (servicePortNum == 0)
                continue;

            ports[servicePortNum] = device.Id();

            // find the upper bounds
            highestOldPortNumber = max(servicePortNum, highestOldPortNumber);
        }

        // find the gaps and close them
        // this isn't super efficient. There may be a better approach
        bool done{ false };
        UINT32 oldPortNumber{ 1 };
        UINT32 newPortNumber{ 1 };

        while (!done)
        {
            // skip the empty slots. There could be many in a row.
            while (ports.find(oldPortNumber) == ports.end() && oldPortNumber <= highestOldPortNumber)
            {
                oldPortNumber++;
            }

            // we've found one. See if it needs renumbering.
            if (oldPortNumber > newPortNumber)
            {
                // we need to renumber this port

                // get the midi1 port with this device interface id
                auto thisPortDeviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(ports[oldPortNumber]);

                auto item = std::find_if(m_midiPorts.begin(), m_midiPorts.end(), [&](const std::unique_ptr<MIDIPORT>& Port)
                    {
                        if (internal::NormalizeEndpointInterfaceIdWStringCopy(std::wstring{ Port->DeviceInterfaceId.get() }) == thisPortDeviceInterfaceId)
                        {
                            return true;
                        }

                        return false;
                    });

                if (item != m_midiPorts.end())
                {
                    // found the matching midi 1 port, so let's set its port number

                    UINT32 assignedPortNumber = newPortNumber;

                    std::vector<DEVPROPERTY> newProperties{ };
                    newProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_ServiceAssignedPortNumber, DEVPROP_STORE_SYSTEM, nullptr},
                                        DEVPROP_TYPE_UINT32, (ULONG)(sizeof(UINT32)), (PVOID)(&(assignedPortNumber)) });

                    RETURN_IF_FAILED(SwDeviceInterfacePropertySet(
                        item->get()->SwDevice.get(),
                        thisPortDeviceInterfaceId.c_str(),
                        static_cast<ULONG>(newProperties.size()),
                        (const DEVPROPERTY*)newProperties.data()));
                }
            }

            newPortNumber++;
            oldPortNumber++;

            done = (oldPortNumber > highestOldPortNumber);
        }

    }

    return S_OK;
}


// TODO: This method should acquire a lock on something to ensure
// we don't get two calls at the same time, given the same port number
_Use_decl_annotations_
HRESULT
CMidiDeviceManager::AssignPortNumber(
    HSWDEVICE SwDevice,
    PWSTR deviceInterfaceId,
    MidiFlow flow
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(deviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    auto thisDeviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(deviceInterfaceId);

    std::map<UINT32, winrt::hstring> ports;

    // for this version, we only check active devices. We're not trying to preserve or reserve numbers
    winrt::hstring deviceSelector(flow == MidiFlowOut ? MIDI1_OUTPUT_DEVICES : MIDI1_INPUT_DEVICES);
    deviceSelector = deviceSelector + L" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(STRING_PKEY_MIDI_ServiceAssignedPortNumber);

    auto deviceList = DeviceInformation::FindAllAsync(deviceSelector, additionalProperties).get();

    // because we compact the numbers on device removal, the first available port number is the device count including us
    // however, we need to exclude devices which are handled specially, like the GS Synth, and also any devices which 
    // are WinRT-only, like BLE MIDI 1.0 devices. Those mess up the count.

    // store all the existing port numbers
    for (auto const& device : deviceList)
    {
        std::wstring id = internal::NormalizeEndpointInterfaceIdWStringCopy(device.Id().c_str());

        // ignore WinRT MIDI 1.0 BLE because WinMM doesn't support them
        if (id.find(L".ble10#") != std::wstring::npos)
        {
            continue;
        }

        // ignore the GS synth
        if (id.find(L"#microsoftgswavetablesynth#") != std::wstring::npos)
        {
            continue;
        }

        // ensure we're not looking at ourselves
        if (id == thisDeviceInterfaceId)
        {
            continue;
        }

        // get the ServiceAssignedPortNumber
        auto servicePortNum = internal::SafeGetSwdPropertyFromDeviceInformation<UINT32>(STRING_PKEY_MIDI_ServiceAssignedPortNumber, device, 0);

        // missing for some reason. We skip it
        if (servicePortNum == 0) 
        {
            continue;
        }

        ports[servicePortNum] = device.Id();
    }

    UINT32 firstAvailablePortNumber{ 1 };

    // now find the first available port number
    for (firstAvailablePortNumber = 1; ports.find(firstAvailablePortNumber) != ports.end(); firstAvailablePortNumber++);

    UINT32 assignedPortNumber = firstAvailablePortNumber;

    // set the new port number
    std::vector<DEVPROPERTY> newProperties{};
    newProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_ServiceAssignedPortNumber, DEVPROP_STORE_SYSTEM, nullptr},
                        DEVPROP_TYPE_UINT32, (ULONG)(sizeof(UINT32)), (PVOID)(&(assignedPortNumber)) });

    RETURN_IF_FAILED(SwDeviceInterfacePropertySet(
        SwDevice,
        deviceInterfaceId,
        1,
        (const DEVPROPERTY*)newProperties.data()));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiDeviceManager::GetFunctionBlockPortInfo(
    LPCWSTR umpDeviceInterfaceId,
    DeviceInformation deviceInfo,
    std::map<UINT32, PORT_INFO> portInfo[2]
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(umpDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    // We should skip this completely if it's not a UMP-native endpoint. Otherwise, we do all
    // this work for MIDI 1.0 devices using the UMP driver
    auto nativeDataFormat = internal::SafeGetSwdPropertyFromDeviceInformation<uint8_t>(STRING_PKEY_MIDI_NativeDataFormat, deviceInfo, 1);
    if (nativeDataFormat != MidiDataFormats::MidiDataFormats_UMP)
    {
        return E_NOTFOUND;
    }


    // we don't process any function blocks until initial discovery has completed
    auto discoveryComplete = internal::SafeGetSwdPropertyFromDeviceInformation<bool>(STRING_PKEY_MIDI_EndpointDiscoveryProcessComplete, deviceInfo, false);
    if (!discoveryComplete)
    {
        return E_NOTFOUND;
    }

    // we've completed initial discovery, so any changes after that are fair game

    auto declaredFunctionBlockCount = internal::SafeGetSwdPropertyFromDeviceInformation<uint8_t>(STRING_PKEY_MIDI_FunctionBlockDeclaredCount, deviceInfo, 0);
    RETURN_HR_IF_EXPECTED(E_NOTFOUND, declaredFunctionBlockCount == 0);

    // now process each available function block
    for (UINT fbi = 0; fbi < declaredFunctionBlockCount && fbi < MIDI_MAX_FUNCTION_BLOCKS; fbi++)
    {
        std::wstring functionBlockBaseString = MIDI_STRING_PKEY_GUID;
        functionBlockBaseString += MIDI_STRING_PKEY_PID_SEPARATOR;
        std::wstring functionBlockString = functionBlockBaseString + std::to_wstring(MIDI_FUNCTION_BLOCK_PROPERTY_INDEX_START + fbi);
        std::wstring functionBlockNameString = functionBlockBaseString + std::to_wstring(MIDI_FUNCTION_BLOCK_NAME_PROPERTY_INDEX_START + fbi);
        std::wstring functionBlockName;

        // This property can be missing if we didn't get a complete list of function blocks from the device
        auto prop = deviceInfo.Properties().Lookup(functionBlockString.c_str());
        if (prop == nullptr)
        {
            continue;
        }

        functionBlockName = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(functionBlockNameString.c_str(), deviceInfo, L"");
        if (functionBlockName.empty())
        {
            functionBlockName = deviceInfo.Name();
        }
        functionBlockName = functionBlockName.substr(0, MAXPNAMELEN - 1);


        auto refArray = winrt::unbox_value<winrt::Windows::Foundation::IReferenceArray<uint8_t>>(prop);
        if (refArray == nullptr)
        {
            continue;
        }

        auto data = refArray.Value();
        auto fb = (MidiFunctionBlockProperty*)(data.data());

        // TODO: this is going to result in slightly misleading names when a function blocks have overlapping groups. Ignoring for now.

        for (UINT i = fb->FirstGroup; i < (UINT)(fb->FirstGroup + fb->NumberOfGroupsSpanned); i++)
        {
            if (fb->Direction == MIDI_FUNCTION_BLOCK_DIRECTION_BLOCK_BIDIRECTIONAL ||
                fb->Direction == MIDI_FUNCTION_BLOCK_DIRECTION_BLOCK_OUTPUT) // message source
            {
                portInfo[MidiFlowIn][i].InUse = true;
                portInfo[MidiFlowIn][i].Flow = (MidiFlow) MidiFlowIn;
                portInfo[MidiFlowIn][i].IsEnabled = portInfo[MidiFlowIn][i].IsEnabled || fb->IsActive;
                portInfo[MidiFlowIn][i].InterfaceId = umpDeviceInterfaceId;
                portInfo[MidiFlowIn][i].Name = functionBlockName;
            }

            if (fb->Direction == MIDI_FUNCTION_BLOCK_DIRECTION_BLOCK_BIDIRECTIONAL ||
                fb->Direction == MIDI_FUNCTION_BLOCK_DIRECTION_BLOCK_INPUT) // message destination
            {
                portInfo[MidiFlowOut][i].InUse = true;
                portInfo[MidiFlowOut][i].Flow = (MidiFlow) MidiFlowOut;
                portInfo[MidiFlowOut][i].IsEnabled = portInfo[MidiFlowOut][i].IsEnabled || fb->IsActive;
                portInfo[MidiFlowOut][i].InterfaceId = umpDeviceInterfaceId;
                portInfo[MidiFlowOut][i].Name = functionBlockName;
            }
        }
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiDeviceManager::GetGroupTerminalBlockPortInfo(
    LPCWSTR umpDeviceInterfaceId,
    DeviceInformation deviceInfo,
    std::map<UINT32, PORT_INFO> portInfo[2]
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(umpDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    auto prop = deviceInfo.Properties().Lookup(STRING_PKEY_MIDI_GroupTerminalBlocks);
    RETURN_HR_IF_EXPECTED(E_NOTFOUND, !prop);

    auto refArray = winrt::unbox_value<winrt::Windows::Foundation::IReferenceArray<uint8_t>>(prop);
    auto refData = refArray.Value();
    PBYTE data = refData.data();
    PKSMULTIPLE_ITEM gtbMultItemHeader = (PKSMULTIPLE_ITEM)(data);

    RETURN_HR_IF(E_UNEXPECTED, gtbMultItemHeader->Count == 0);

    PBYTE gtbMemory = (data + sizeof(KSMULTIPLE_ITEM));
    PBYTE nextGtb = gtbMemory;
    for (UINT gti = 0; gti < gtbMultItemHeader->Count; gti++)
    {
        PUMP_GROUP_TERMINAL_BLOCK_DEFINITION gtb = (PUMP_GROUP_TERMINAL_BLOCK_DEFINITION)gtbMemory;
        nextGtb = gtbMemory + gtb->GrpTrmBlock.Size;

        for (UINT i = gtb->GrpTrmBlock.FirstGroupIndex; i < (UINT)(gtb->GrpTrmBlock.FirstGroupIndex + gtb->GrpTrmBlock.GroupCount); i++)
        {
            auto gtbName = std::wstring(gtb->Name).substr(0, MAXPNAMELEN - 1);

            // the names here will be used only if the name table comes up empty

            if (gtbName.empty())
            {
                gtbName = std::wstring(deviceInfo.Name().c_str()).substr(0, MAXPNAMELEN - 1);
            }

            // If the flow of this gtb matches the flow we're processing
            if (gtb->GrpTrmBlock.Direction == MIDI_GROUP_TERMINAL_BLOCK_BIDIRECTIONAL ||
                gtb->GrpTrmBlock.Direction == MIDI_GROUP_TERMINAL_BLOCK_OUTPUT)
            {
                portInfo[MidiFlowIn][i].InUse = true;
                portInfo[MidiFlowIn][i].IsEnabled = true;
                portInfo[MidiFlowIn][i].Flow = (MidiFlow) MidiFlowIn;
                portInfo[MidiFlowIn][i].InterfaceId = umpDeviceInterfaceId;
                portInfo[MidiFlowIn][i].Name = gtbName;
            }

            if (gtb->GrpTrmBlock.Direction == MIDI_GROUP_TERMINAL_BLOCK_BIDIRECTIONAL ||
                gtb->GrpTrmBlock.Direction == MIDI_GROUP_TERMINAL_BLOCK_INPUT)
            {
                portInfo[MidiFlowOut][i].InUse = true;
                portInfo[MidiFlowOut][i].IsEnabled = true;
                portInfo[MidiFlowOut][i].Flow = (MidiFlow) MidiFlowOut;
                portInfo[MidiFlowOut][i].InterfaceId = umpDeviceInterfaceId;
                portInfo[MidiFlowOut][i].Name = gtbName;
            }
        }

        gtbMemory = nextGtb;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiDeviceManager::UseFallbackMidi1PortDefinition(
    LPCWSTR umpDeviceInterfaceId,
    DeviceInformation deviceInfo,
    std::map<UINT32, PORT_INFO> portInfo[2]
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(umpDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    // If protocol negotation has completed, and we still don't have the required properties,
    // then we need to push through with creating the fallback midi 1 ports. We'll create only
    // one port in each direction because this is a fallback case that really should happen
    // only if a device is not bothering to provide function blocks. Creating 16 in each direction
    // just results in a lot of WinMM port clutter

    // we don't process any function blocks until initial discovery has completed
    auto discoveryComplete = internal::SafeGetSwdPropertyFromDeviceInformation<bool>(STRING_PKEY_MIDI_EndpointDiscoveryProcessComplete, deviceInfo, false);
    if (discoveryComplete)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Discovery has completed. Using fallback WinMM port definitions", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(umpDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)

        );

        for (UINT flow = 0; flow < 2; flow++)
        {
            for (UINT groupIndex = 0; IS_VALID_GROUP_INDEX(groupIndex); groupIndex++)
            {
                portInfo[flow][groupIndex].InUse = true;
                portInfo[flow][groupIndex].IsEnabled = (bool)(groupIndex == 0);     // only enabled for first group
                portInfo[flow][groupIndex].Flow = (MidiFlow)flow;
                portInfo[flow][groupIndex].InterfaceId = umpDeviceInterfaceId;
                portInfo[flow][groupIndex].Name = std::wstring(deviceInfo.Name().c_str()).substr(0, MAXPNAMELEN - 1);
            }
        }
    }
    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiDeviceManager::RebuildAndUpdateNameTableForMidi2EndpointWithFunctionBlocks(
    LPCWSTR umpDeviceInterfaceId,
    winrt::Windows::Devices::Enumeration::DeviceInformation deviceInfo,
    PMIDIPORT umpMidiPort

)
{
    // this is called after protocol negotiation and discovery have completed, so we can
    // generate names based on function blocks, not just on group terminal blocks.

    // this is only needed for MIDI 2.0 UMP devices, but among those are not just USB,
    // but also virtual, loopback, network, etc.


    // read all function blocks, and figure out names and flow
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(umpDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );


    auto declaredFunctionBlockCount = internal::SafeGetSwdPropertyFromDeviceInformation<uint8_t>(STRING_PKEY_MIDI_FunctionBlockDeclaredCount, deviceInfo, 0);
    RETURN_HR_IF_EXPECTED(E_NOTFOUND, declaredFunctionBlockCount == 0);

    auto currentNameTable = WindowsMidiServicesNamingLib::MidiEndpointNameTable::FromDeviceInfo(deviceInfo);
    WindowsMidiServicesNamingLib::MidiEndpointNameTable newNameTable{};

    auto deviceName = deviceInfo.Name();

    uint8_t outIndex{ 0 };
    uint8_t inIndex{ 0 };

    // now process each available function block
    for (UINT fbi = 0; fbi < declaredFunctionBlockCount && fbi < MIDI_MAX_FUNCTION_BLOCKS; fbi++)
    {
        std::wstring functionBlockBaseString = MIDI_STRING_PKEY_GUID;
        functionBlockBaseString += MIDI_STRING_PKEY_PID_SEPARATOR;
        std::wstring functionBlockString = functionBlockBaseString + std::to_wstring(MIDI_FUNCTION_BLOCK_PROPERTY_INDEX_START + fbi);
        std::wstring functionBlockNameString = functionBlockBaseString + std::to_wstring(MIDI_FUNCTION_BLOCK_NAME_PROPERTY_INDEX_START + fbi);
        std::wstring functionBlockName;

        // This property can be missing if we didn't get a complete list of function blocks from the device
        auto prop = deviceInfo.Properties().Lookup(functionBlockString.c_str());
        if (prop == nullptr)
        {
            continue;
        }

        functionBlockName = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(functionBlockNameString.c_str(), deviceInfo, L"");
        if (functionBlockName.empty())
        {
            functionBlockName = deviceInfo.Name();
        }
        functionBlockName = functionBlockName.substr(0, MAXPNAMELEN - 1);


        auto refArray = winrt::unbox_value<winrt::Windows::Foundation::IReferenceArray<uint8_t>>(prop);
        if (refArray == nullptr)
        {
            continue;
        }

        auto data = refArray.Value();
        auto fb = (MidiFunctionBlockProperty*)(data.data());

        for (uint8_t groupIndex = fb->FirstGroup; groupIndex < fb->FirstGroup + fb->NumberOfGroupsSpanned; groupIndex++)
        {
            if (fb->Direction == MIDI_FUNCTION_BLOCK_DIRECTION_BLOCK_BIDIRECTIONAL ||
                fb->Direction == MIDI_FUNCTION_BLOCK_DIRECTION_BLOCK_OUTPUT) // message source
            {
                newNameTable.PopulateEntryForNativeUmpDevice(
                    groupIndex, 
                    MidiFlow::MidiFlowIn, // a MIDI Source
                    currentNameTable->GetSourceEntryCustomName(groupIndex),
                    deviceName.c_str(),
                    deviceName.c_str(),
                    functionBlockName,
                    outIndex);

                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Added midi input port name from function block name", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingUInt16(groupIndex, "group index"),
                    TraceLoggingWideString(functionBlockName.c_str(), "gtb name")
                );

                outIndex++;
            }

            if (fb->Direction == MIDI_FUNCTION_BLOCK_DIRECTION_BLOCK_BIDIRECTIONAL ||
                fb->Direction == MIDI_FUNCTION_BLOCK_DIRECTION_BLOCK_INPUT) // message destination
            {
                newNameTable.PopulateEntryForNativeUmpDevice(
                    groupIndex,
                    MidiFlow::MidiFlowOut, // a MIDI destination
                    currentNameTable->GetDestinationEntryCustomName(groupIndex),
                    deviceName.c_str(),
                    deviceName.c_str(),
                    functionBlockName,
                    inIndex);

                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Added midi input port name from function block name", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingUInt16(groupIndex, "group index"),
                    TraceLoggingWideString(functionBlockName.c_str(), "gtb name")
                );

                inIndex++;
            }
        }
    }

    std::vector<DEVPROPERTY> interfaceDevProperties{ };

    if (!newNameTable.IsEqualTo(currentNameTable.get()))
    {
        newNameTable.WriteProperties(interfaceDevProperties);

        // data has changed, so write it
        auto deviceHandle = umpMidiPort->SwDevice.get();

        auto propSetHR = SwDeviceInterfacePropertySet(
            deviceHandle,
            umpMidiPort->DeviceInterfaceId.get(),
            static_cast<ULONG>(interfaceDevProperties.size()),
            static_cast<const DEVPROPERTY*>(interfaceDevProperties.data())
        );

        RETURN_IF_FAILED(propSetHR);
    }

    return S_OK;
}






_Use_decl_annotations_
HRESULT
CMidiDeviceManager::GetMidi1PortNames(
    winrt::Windows::Devices::Enumeration::DeviceInformation deviceInfo,
    std::map<UINT32, PORT_INFO> portInfo[2]
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(deviceInfo.Name().c_str(), "parent device")
        );

    // get the names from that parent endpoint

    WindowsMidiServicesNamingLib::Midi1PortNameSelection namingSelection{ };
    WindowsMidiServicesNamingLib::Midi1PortNameSelection defaultNamingSelection = 
        WindowsMidiServicesNamingLib::MidiEndpointNameTable::GetSystemDefaultPortNameSelection();

    namingSelection = static_cast<WindowsMidiServicesNamingLib::Midi1PortNameSelection>(internal::SafeGetSwdPropertyFromDeviceInformation<uint32_t>(
        STRING_PKEY_MIDI_Midi1PortNamingSelection,
        deviceInfo,
        WindowsMidiServicesNamingLib::Midi1PortNameSelection::UseGlobalDefault));

    // this handles garbage data values
    if (namingSelection != WindowsMidiServicesNamingLib::Midi1PortNameSelection::UseLegacyWinMM &&
        namingSelection != WindowsMidiServicesNamingLib::Midi1PortNameSelection::UseNewStyleName)
    {
        namingSelection = defaultNamingSelection;
    }

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Retrieved port name selection", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(deviceInfo.Name().c_str(), "parent device"),
        TraceLoggingUInt32(static_cast<uint32_t>(namingSelection), "name selection enum")
    );

    // get the existing name table. If the endpoint has no name table, this can result
    // in an empty name.
    auto nameTable = WindowsMidiServicesNamingLib::MidiEndpointNameTable::FromDeviceInfo(deviceInfo);

    for (auto const& flow : { MidiFlow::MidiFlowIn, MidiFlow::MidiFlowOut })
    {
        for (auto& portInfoEntry : portInfo[flow])
        {
            portInfoEntry.second.Name = nameTable->GetPreferredName(
                static_cast<uint8_t>(portInfoEntry.first),
                flow,
                namingSelection
            );
        }
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiDeviceManager::RebuildMidi1PortsForEndpoint(
    LPCWSTR endpointDeviceInterfaceId
)
{
    std::wstring cleanEndpointId { internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId) };

    if (Feature_Servicing_MIDI2DeviceRemoval::IsEnabled())
    {
        auto lock = m_midiPortsLock.lock();

        for (auto& port : m_midiPorts)
        {
            std::wstring cleanPortId { port->DeviceInterfaceId.get() };
            cleanPortId = internal::NormalizeEndpointInterfaceIdWStringCopy(cleanPortId);

            if (cleanPortId == cleanEndpointId)
            {
                RETURN_IF_FAILED(SyncMidi1Ports(port.get()));

                return S_OK;
            }
        }
    }
    else
    {
        for (auto& port : m_midiPorts)
        {
            std::wstring cleanPortId { port->DeviceInterfaceId.get() };
            cleanPortId = internal::NormalizeEndpointInterfaceIdWStringCopy(cleanPortId);
        
            if (cleanPortId == cleanEndpointId)
            {
                RETURN_IF_FAILED(SyncMidi1Ports(port.get()));
        
                return S_OK;
            }
        }
    }

    return E_NOTFOUND;
}


// TODO: This is in support of DRV_QUERYDEVICEINTERFACE calls in WinMM
// Expects the umpEndpointDeviceInfo object to have included the properties
// STRING_PKEY_MIDI_DriverDeviceInterface
// STRING_DEVPKEY_KsAggMidiGroupPinMap
_Use_decl_annotations_
std::wstring CMidiDeviceManager::GetWinMMDeviceInterfaceIdForGroupFromParentEndpoint(
    winrt::Windows::Devices::Enumeration::DeviceInformation const& umpEndpointDeviceInfo,
    uint8_t const groupIndex,
    MidiFlow const flowFromBlockPerspective
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt8(groupIndex, "group index"),
        TraceLoggingUInt32(flowFromBlockPerspective, "flow from block perspective"),
        TraceLoggingWideString(umpEndpointDeviceInfo.Id().c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );


    // this is our fallback value: the parent endpoint's Id. This is likely to be useful only
    // for non-USB transports
    std::wstring deviceInterfaceId { };

    KSAPinMapEntryInternal pinMapEntry{ };

    // first, check to see if we have a pin map entry, which is created by KSA
    if (SUCCEEDED(GetPinMapEntry(umpEndpointDeviceInfo, groupIndex, flowFromBlockPerspective, pinMapEntry)) &&
        !pinMapEntry.FilterId.empty())
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Using filter Id from pin map entry", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(pinMapEntry.FilterId.c_str(), "filter id"),
            TraceLoggingWideString(umpEndpointDeviceInfo.Id().c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        deviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(pinMapEntry.FilterId);
    }
    else
    {
        // no pin map entry. check to see if we have STRING_PKEY_MIDI_DriverDeviceInterface

        auto id = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(STRING_PKEY_MIDI_DriverDeviceInterface, umpEndpointDeviceInfo, L"");

        if (!id.empty())
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Using filter Id from single property", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(id.c_str(), "filter id"),
                TraceLoggingWideString(umpEndpointDeviceInfo.Id().c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

            deviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(id.c_str());
        }
    }


    if (internal::NormalizeEndpointInterfaceIdWStringCopy(deviceInterfaceId).empty())
    {
        deviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(umpEndpointDeviceInfo.Id().c_str());
    }

    return deviceInterfaceId;
}



// TODO: If any of the ports in play here are actually in-use by WinMM or WinRT MIDI 1.0
// we need to leave them alone for now. Otherwise, tearing down and rebuilding a port
// like that just because a function block came through will end up being a problem.
_Use_decl_annotations_
HRESULT
CMidiDeviceManager::SyncMidi1Ports(
    PMIDIPORT umpMidiPort
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(umpMidiPort->AssociatedInterfaceId.c_str(), "associated interface id"),
        TraceLoggingWideString(umpMidiPort->DeviceInterfaceId.get(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    if (!m_CreateMidi1Ports)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Exit: Midi 1 ports owned by endpoint builder.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(umpMidiPort->AssociatedInterfaceId.c_str(), "associated interface id"),
            TraceLoggingWideString(umpMidiPort->DeviceInterfaceId.get(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        return S_OK;
    }

    HRESULT hrTemp;

    // if this port is ump only, then no additional work is
    // necessary.
    RETURN_HR_IF(S_OK, umpMidiPort->UmpOnly);

    std::map<UINT32, PORT_INFO> portInfo[2]{ };

    // Retrieve all of the needed ump port properties, for efficiency get
    // them all at once to avoid repeated CreateFromIdAsync calls.
    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(STRING_PKEY_MIDI_CustomEndpointName);
    additionalProperties.Append(STRING_PKEY_MIDI_ManufacturerName);
    additionalProperties.Append(STRING_PKEY_MIDI_EndpointName);
    additionalProperties.Append(STRING_PKEY_MIDI_TransportLayer);
    additionalProperties.Append(STRING_PKEY_MIDI_SupportedDataFormats);
    additionalProperties.Append(STRING_PKEY_MIDI_NativeDataFormat);
    additionalProperties.Append(STRING_PKEY_MIDI_FunctionBlockDeclaredCount);
    additionalProperties.Append(STRING_PKEY_MIDI_GroupTerminalBlocks);
    additionalProperties.Append(STRING_PKEY_MIDI_EndpointDiscoveryProcessComplete);

    //additionalProperties.Append(STRING_PKEY_MIDI_CustomPortAssignments);
    
    additionalProperties.Append(STRING_PKEY_MIDI_Midi1PortNamingSelection);
    additionalProperties.Append(STRING_PKEY_MIDI_Midi1PortNameTable);
    
    additionalProperties.Append(STRING_DEVPKEY_KsAggMidiGroupPinMap);       // need this pin map to find filter id
    additionalProperties.Append(STRING_PKEY_MIDI_DriverDeviceInterface);    // when no pin map is used, this has the filter id

    // We have function blocks to retrieve
    // build up the property keys to query for the function blocks
    std::wstring functionBlockBaseString = MIDI_STRING_PKEY_GUID;
    functionBlockBaseString += MIDI_STRING_PKEY_PID_SEPARATOR;
    for (UINT fbi = 0; fbi < MIDI_MAX_FUNCTION_BLOCKS; fbi++)
    {
        std::wstring functionBlockString = functionBlockBaseString + std::to_wstring(MIDI_FUNCTION_BLOCK_PROPERTY_INDEX_START + fbi);
        std::wstring functionBlockNameString = functionBlockBaseString + std::to_wstring(MIDI_FUNCTION_BLOCK_NAME_PROPERTY_INDEX_START + fbi);
        additionalProperties.Append(functionBlockString.c_str());
        additionalProperties.Append(functionBlockNameString.c_str());
    }

    auto deviceInfo = DeviceInformation::CreateFromIdAsync(
        umpMidiPort->DeviceInterfaceId.get(), 
        additionalProperties, 
        winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();


    std::wstring thisUmpMidiPortDeviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(umpMidiPort->DeviceInterfaceId.get());

    // get the current function block information from the UMP SWD, if present, else fall back to
    // GTB if that is present. If neither, then there's nothing more to do.

    // GetFunctionBlockPortInfo will return E_NOTFOUND if it's a bytestream device, if discovery hasn't completed, or if no function blocks
    hrTemp = GetFunctionBlockPortInfo(thisUmpMidiPortDeviceInterfaceId.c_str(), deviceInfo, portInfo);
    RETURN_HR_IF(hrTemp, FAILED(hrTemp) && E_NOTFOUND != hrTemp);
    if (E_NOTFOUND == hrTemp)
    {
        hrTemp = GetGroupTerminalBlockPortInfo(thisUmpMidiPortDeviceInterfaceId.c_str(), deviceInfo, portInfo);
        RETURN_HR_IF(hrTemp, FAILED(hrTemp) && E_NOTFOUND != hrTemp);
        if (E_NOTFOUND == hrTemp)
        {
            hrTemp = UseFallbackMidi1PortDefinition(thisUmpMidiPortDeviceInterfaceId.c_str(), deviceInfo, portInfo);
            RETURN_IF_FAILED(hrTemp);
        }
    }
    else
    {
        // we have function blocks, so rebuild the name table using them. We could potentially
        // optimize this a bit more by only triggering if FB-specific properties have changed

        // update name table, and write it out to the property if it has changed
        hrTemp = RebuildAndUpdateNameTableForMidi2EndpointWithFunctionBlocks(thisUmpMidiPortDeviceInterfaceId.c_str(), deviceInfo, umpMidiPort);
        RETURN_HR_IF(hrTemp, FAILED(hrTemp) && E_NOTFOUND != hrTemp);
    }

    auto transportId = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::guid>(STRING_PKEY_MIDI_TransportLayer, deviceInfo, GUID_NULL);
    auto nativeDataFormat = (MidiDataFormats)internal::SafeGetSwdPropertyFromDeviceInformation<uint8_t>(STRING_PKEY_MIDI_NativeDataFormat, deviceInfo, MidiDataFormats::MidiDataFormats_Invalid);

    // we need to know if this is a native UMP device on a UMP transport, a byte format device on a UMP transport, or a 
    // byte format device on a byte format transport. We should probably get this from other properties rather than
    // having a dependency in here on the GUID for the KSA transport

    // we know which groups are active, so now we get the names for these groups
    LOG_IF_FAILED(GetMidi1PortNames(deviceInfo, portInfo));

    // First walk the midi ports list, identifying ports that have already been
    // created, updating the assigned custom number, deactivating any
    // which were previously active but no longer active due to a function block
    // change.

    bool portsErased{ false };
    for (auto midiPort = m_midiPorts.begin(); midiPort != m_midiPorts.end();)
    {
        if (midiPort->get()->MidiOne && 
            internal::NormalizeEndpointInterfaceIdWStringCopy(midiPort->get()->AssociatedInterfaceId) == thisUmpMidiPortDeviceInterfaceId)
        {
            // If this port is a midi 1 port, and is associated to the parent UMP device
            // we are processing, we need to process it.

            // We know that it's the device interface id for this group index
            portInfo[midiPort->get()->Flow][midiPort->get()->GroupIndex].InterfaceId = midiPort->get()->DeviceInterfaceId.get();

            // if it's supposed to be disabled, then disable it, and remove the port from
            // the system.
            if (!portInfo[midiPort->get()->Flow][midiPort->get()->GroupIndex].IsEnabled)
            {
                LOG_IF_FAILED(SwDeviceInterfaceSetState(midiPort->get()->SwDevice.get(), midiPort->get()->DeviceInterfaceId.get(), FALSE));
                midiPort = m_midiPorts.erase(midiPort);
                portsErased = true;
                continue;
            }
        }

        midiPort++;
    }

    if (portsErased)
    {
        LOG_IF_FAILED(CompactPortNumbers());
    }


    std::vector<DEVPROPERTY> interfaceProperties{};
    SW_DEVICE_CREATE_INFO createInfo{};
    GUID transportGUID{};
    MidiDataFormats dataFormats{};
    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszInstanceId = umpMidiPort->InstanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;

    if (GUID_NULL != umpMidiPort->ContainerId)
    {
        createInfo.pContainerId = &(umpMidiPort->ContainerId);
    }

    for (UINT flow = 0; flow <= 1; flow++)
    {
        for (UINT groupIndex = 0; IS_VALID_GROUP_INDEX(groupIndex); groupIndex++)
        {
            if (portInfo[flow][groupIndex].IsEnabled && !portInfo[flow][groupIndex].InterfaceId.empty())
            {
                if (interfaceProperties.empty())
                {
                    // This is the first creation, so we need to do some additional work to prepare.
                    // Retrieve all of the required properties from the UMP interface and save them in the
                    // vector.

                    interfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_TransportLayer, DEVPROP_STORE_SYSTEM, nullptr},
                        DEVPROP_TYPE_GUID, (ULONG)(sizeof(GUID)), (PVOID)(&(transportId)) });

                    interfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_NativeDataFormat, DEVPROP_STORE_SYSTEM, nullptr},
                        DEVPROP_TYPE_BYTE, (ULONG)(sizeof(BYTE)), (PVOID)(&(nativeDataFormat)) });

                    auto prop = deviceInfo.Properties().Lookup(STRING_PKEY_MIDI_SupportedDataFormats);
                    if (prop)
                    {
                        dataFormats = (MidiDataFormats) winrt::unbox_value<uint8_t>(prop);
                        interfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_SupportedDataFormats, DEVPROP_STORE_SYSTEM, nullptr},
                            DEVPROP_TYPE_BYTE, (ULONG)(sizeof(BYTE)), (PVOID)(&(dataFormats)) });
                    }
                }


                // name we created earlier in the GetMidi1PortNames function
                std::wstring friendlyName = portInfo[flow][groupIndex].Name;

                if (friendlyName.empty())
                {
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"No name provided", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(umpMidiPort->DeviceInterfaceId.get(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                        TraceLoggingUInt32(flow, "flow"),
                        TraceLoggingUInt32(groupIndex, "group index")
                        );

                    // -----------------------------------------------------------------------------------------------------------------------------
                    // this is just a last-chance fallback. If we ever see this in production, that's a problem.

                    if (groupIndex == 0)
                    {
                        friendlyName = std::wstring{ deviceInfo.Name() }.substr(0, MAXPNAMELEN - 1);
                    }
                    else
                    {
                        friendlyName = std::wstring{ deviceInfo.Name() }.substr(0, MAXPNAMELEN - 7) + L" Gr " + std::to_wstring(groupIndex + 1);
                    }

                    // -----------------------------------------------------------------------------------------------------------------------------
                }

                interfaceProperties.push_back(DEVPROPERTY{ {DEVPKEY_DeviceInterface_FriendlyName, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (wcslen(friendlyName.c_str()) + 1)), (PVOID)friendlyName.c_str() });

                // we're no longer using custom port numbers
                interfaceProperties.push_back({{ PKEY_MIDI_CustomPortNumber, DEVPROP_STORE_SYSTEM, nullptr },
                    DEVPROP_TYPE_EMPTY, 0, NULL });

                // required by WinMM. Change this property key if it's not the final one used
                std::wstring winmmDeviceInterfaceId = GetWinMMDeviceInterfaceIdForGroupFromParentEndpoint(
                    deviceInfo,
                    static_cast<uint8_t>(groupIndex),
                    flow == MidiFlow::MidiFlowIn ? MidiFlow::MidiFlowOut : MidiFlow::MidiFlowIn // this is from the pin/group perspective
                );

                interfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_DriverDeviceInterface, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (wcslen(winmmDeviceInterfaceId.c_str()) + 1)), (PVOID)winmmDeviceInterfaceId.c_str() });

                // Finally, add the group index that is assigned to this interface
                interfaceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_PortAssignedGroupIndex, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_UINT32, (ULONG)(sizeof(UINT32)), (PVOID)(&groupIndex) });

                // create the swd with the specialized friendly name
                createInfo.pszDeviceDescription = friendlyName.c_str();

                PMIDIPORT createdMidiPort {};

                // create the midi 1 interface
                RETURN_IF_FAILED(ActivateEndpointInternal(
                    umpMidiPort->ParentInstanceId.c_str(),
                    umpMidiPort->DeviceInterfaceId.get(),
                    TRUE,
                    groupIndex,
                    (MidiFlow) flow,
                    (ULONG)interfaceProperties.size(),
                    0,
                    (DEVPROPERTY*)(interfaceProperties.data()),
                    nullptr,
                    (SW_DEVICE_CREATE_INFO*)&createInfo,
                    nullptr,
                    &createdMidiPort));

                // Assign the new port number.
                RETURN_IF_FAILED(AssignPortNumber(createdMidiPort->SwDevice.get(), createdMidiPort->DeviceInterfaceId.get(), createdMidiPort->Flow));

                // We want to reuse interfaceProperties for the next creation,
                // so pop the endpoint specific properties off in preparation.

                interfaceProperties.pop_back(); // customized endpoint name
                interfaceProperties.pop_back(); // WinMM Driver Device Interface (parent Filter id)
                interfaceProperties.pop_back(); // custom port number
                interfaceProperties.pop_back(); // group index
            }
        }
    }

    return S_OK;
}
