// Copyright (c) Microsoft Corporation. All rights reserved.

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <wil\com.h>
#include <wil\resource.h>
#include <wil/resource.h>
#include <setupapi.h>
#include <cfgmgr32.h>

#include <WexTestClass.h>

#include "MidiTestCommon.h"

#include <wil\registry.h>

#include "MidiDefs.h"

#include "FeatureStaging-MIDI2.h"
#include "Feature_Servicing_MIDI2WaveAPIFix.h"
#include "Feature_Servicing_MIDI2SendTimeout.h"
#include "Feature_Servicing_MIDI2DeviceRemoval.h"
#include "Feature_Servicing_USBMIDI2Spinlock.h"

#include "Feature_Servicing_MIDI2WinmmNoBufs.h"
#include "Feature_Servicing_MIDI2MultipleGroups.h"
#include "Feature_Servicing_MIDI2FilterCreations.h"

#include "Feature_Servicing_MIDI2VirtualPortDriversFix.h"
#include "Feature_Servicing_MIDI2RTTimestamp.h"
#include "Feature_Servicing_MIDI2WinmmAddBufferSizeCheck.h"
#include "Feature_Servicing_MIDI2ContainerIds.h"

using unique_hdevinfo = wil::unique_any_handle_invalid<decltype(&::SetupDiDestroyDeviceInfoList), ::SetupDiDestroyDeviceInfoList>;

// Default assume group 0 for the messages, tests are responsible for changing the
// group number of the test message to align to the ports being used for testing.
UMP32 g_MidiTestData_32 = {0x20AA1234 }; // translates to midi 1 message 0xAA 0x12 0x34
UMP64 g_MidiTestData_64 = {0x40917000, 0x48000000 }; // note on message that translates to g_MidiTestMessage
UMP96 g_MidiTestData_96 = {0xb1CC1234, 0xbaadf00d, 0xdeadbeef }; // does not translate to midi 1
UMP128 g_MidiTestData_128 = {0xF1DD1234, 0xbaadf00d, 0xdeadbeef, 0xd000000d }; // does not translate to midi 1

// note on channel 1, note 0x70, velocity 0x42
MIDI_MESSAGE g_MidiTestMessage = { 0x91, 0x70, 0x42 }; // translates to UMP 0x40917000, 0x48000000

#define SERVICE_STOP_TIMEOUT  60000
#define SERVICE_START_TIMEOUT 60000

#define MIDI_SERVICE_NAME L"MidiSrv"

_Use_decl_annotations_
void PrintMidiMessage(PVOID payload, UINT32 payloadSize, UINT32 expectedPayloadSize, LONGLONG payloadPosition)
{
    if (payloadSize < expectedPayloadSize)
    {
        LOG_OUTPUT(L"INCOMING: PayloadSize %d < ExpectedPayloadSize %d", payloadSize, expectedPayloadSize);
    }
    
    LOG_OUTPUT(L"Payload position is %I64d", payloadPosition);

    if (payloadSize == sizeof(MIDI_MESSAGE))
    {
        // if it's bytestream, print it
        MIDI_MESSAGE *data = (MIDI_MESSAGE *) payload;
        LOG_OUTPUT(L"INCOMING: status 0x%hx data1 0x%hx data2 0x%hx - size 0x%08x", data->status, data->data1, data->data2, payloadSize);
    }
    else if (payloadSize >= sizeof(UMP32))
    {
        // if it's UMP
        UMP128 *data = (UMP128 *) payload;
        LOG_OUTPUT(L"INCOMING: data 1 0x%08x - size 0x%08x", data->data1, payloadSize);
        if (payloadSize >= sizeof(UMP64))
        {
            LOG_OUTPUT(L"INCOMING: data 2 0x%08x", data->data2);
            if (payloadSize >= sizeof(UMP96))
            {
                LOG_OUTPUT(L"INCOMING: data 3 0x%08x", data->data3);
                if (payloadSize >= sizeof(UMP128))
                {
                    LOG_OUTPUT(L"INCOMING: data 4 0x%08x", data->data4);
                    if (payloadSize > sizeof(UMP128))
                    {
                        LOG_OUTPUT(L"INCOMING: Buffer contains additional 0x%08x bytes of data", (UINT32) (payloadSize - sizeof(UMP128)));
                    }
                }
            }
        }
    }
}

#define WIDEN2_MACRO(x) L##x
#define WIDEN_MACRO(x)  WIDEN2_MACRO(#x)

#define LOG_FEATURE_STATE(FeatureType)                                     \
    do {                                                                   \
        if (FeatureType::IsEnabled())                                      \
            LOG_OUTPUT(WIDEN_MACRO(FeatureType) L"::IsEnabled()");         \
        else                                                               \
            LOG_OUTPUT(WIDEN_MACRO(FeatureType) L" is not enabled");       \
    } while (0)

void PrintStagingStates()
{
    // 2602
    LOG_FEATURE_STATE(Feature_MIDI2);
    LOG_FEATURE_STATE(Feature_Servicing_MIDI2WaveAPIFix);
    LOG_FEATURE_STATE(Feature_Servicing_MIDI2SendTimeout);
    LOG_FEATURE_STATE(Feature_Servicing_MIDI2DeviceRemoval);
    LOG_FEATURE_STATE(Feature_Servicing_USBMIDI2Spinlock);

    // 2603
    LOG_FEATURE_STATE(Feature_Servicing_MIDI2WinmmNoBufs);
    LOG_FEATURE_STATE(Feature_Servicing_MIDI2MultipleGroups);
    LOG_FEATURE_STATE(Feature_Servicing_MIDI2FilterCreations);

    // 2604
    LOG_FEATURE_STATE(Feature_Servicing_MIDI2VirtualPortDriversFix);
    LOG_FEATURE_STATE(Feature_Servicing_MIDI2RTTimestamp);
    LOG_FEATURE_STATE(Feature_Servicing_MIDI2WinmmAddBufferSizeCheck);
    LOG_FEATURE_STATE(Feature_Servicing_MIDI2ContainerIds);
}

HRESULT StartMIDIService()
{
    wil::unique_schandle serviceControlManager;
    wil::unique_schandle midiService;
    SERVICE_STATUS_PROCESS serviceStatusProcess {0};
    DWORD size_needed {0};

    LOG_OUTPUT(L"Attempting to start [midisrv]");

    serviceControlManager.reset(OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT));
    RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), !serviceControlManager.get());

    midiService.reset(OpenService(serviceControlManager.get(), MIDI_SERVICE_NAME, SERVICE_START | SERVICE_QUERY_STATUS));
    RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), !midiService.get());

    RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), !QueryServiceStatusEx(midiService.get(), SC_STATUS_PROCESS_INFO, (LPBYTE)(&serviceStatusProcess), sizeof(serviceStatusProcess), &size_needed));
    RETURN_HR_IF(S_OK, serviceStatusProcess.dwCurrentState == SERVICE_RUNNING);

    RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), !StartService(midiService.get(), 0, nullptr));

    ULONGLONG startTime = GetTickCount64();
    ULONGLONG currentTime = startTime;
    do
    {
        Sleep(500);
        currentTime = GetTickCount64();
        memset(&serviceStatusProcess, 0, sizeof(serviceStatusProcess));
        size_needed = 0;
        RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), !QueryServiceStatusEx(midiService.get(), SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&serviceStatusProcess), sizeof(serviceStatusProcess), &size_needed));
    }
    while (serviceStatusProcess.dwCurrentState != SERVICE_RUNNING && (currentTime - startTime < SERVICE_START_TIMEOUT));

    RETURN_HR_IF(E_FAIL, serviceStatusProcess.dwCurrentState != SERVICE_RUNNING);

    return S_OK;
}

HRESULT StopMIDIService()
{
    wil::unique_schandle serviceControlManager;
    wil::unique_schandle midiService;
    SERVICE_STATUS_PROCESS serviceStatusProcess {0};
    SERVICE_STATUS serviceStatus{0};
    DWORD size_needed {0};

    LOG_OUTPUT(L"Attempting to stop [midisrv]");

    serviceControlManager.reset(OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT));
    RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), !serviceControlManager.get());

    midiService.reset(OpenService(serviceControlManager.get(), MIDI_SERVICE_NAME, SERVICE_STOP | SERVICE_QUERY_STATUS));
    RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), !midiService.get());

    RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), !QueryServiceStatusEx(midiService.get(), SC_STATUS_PROCESS_INFO, (LPBYTE)(&serviceStatusProcess), sizeof(serviceStatusProcess), &size_needed));
    RETURN_HR_IF(S_OK, serviceStatusProcess.dwCurrentState == SERVICE_STOPPED);

    RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), !ControlService(midiService.get(), SERVICE_CONTROL_STOP, &serviceStatus));

    ULONGLONG startTime = GetTickCount64();
    ULONGLONG currentTime = startTime;
    do
    {
        Sleep(500);
        currentTime = GetTickCount64();
        memset(&serviceStatusProcess, 0, sizeof(serviceStatusProcess));
        size_needed = 0;
        RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), !QueryServiceStatusEx(midiService.get(), SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&serviceStatusProcess), sizeof(serviceStatusProcess), &size_needed));
    }
    while (serviceStatusProcess.dwCurrentState != SERVICE_STOPPED && (currentTime - startTime < SERVICE_STOP_TIMEOUT));

    RETURN_HR_IF(E_FAIL, serviceStatusProcess.dwCurrentState != SERVICE_STOPPED);

    return S_OK;
}

HRESULT SetMidiDiscovery(bool requestedState)
{
    RETURN_IF_FAILED(wil::reg::set_value_dword_nothrow(HKEY_LOCAL_MACHINE, MIDI_ROOT_REG_KEY, MIDI_DISCOVERY_ENABLED_REG_VALUE, static_cast<DWORD>(requestedState)));
    return S_OK;
}

HRESULT
GetInstanceIdFromFilterName
(
    _In_ PCWSTR filterName,
    _Out_ std::wstring& instanceIdOut
)
{
    instanceIdOut.clear();

    // Use KSCATEGORY_AUDIO_DEVICE_INTERFACE GUID
    static const GUID InterfaceCategory = {0x6994ad04, 0x93ef, 0x11d0, {0xa3, 0xcc, 0x0, 0xa0, 0xc9, 0x22, 0x31, 0x96}};

    auto devInfo = unique_hdevinfo{SetupDiGetClassDevs(&InterfaceCategory, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)};
    RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), !devInfo);

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData = {};
    deviceInterfaceData.cbSize = sizeof(deviceInterfaceData);

    for (DWORD index = 0; SetupDiEnumDeviceInterfaces(devInfo.get(), nullptr, &InterfaceCategory, index, &deviceInterfaceData); ++index)
    {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(devInfo.get(), &deviceInterfaceData, nullptr, 0, &requiredSize, nullptr);

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            continue;
        }

        std::unique_ptr<BYTE[]> detailBuffer = std::make_unique<BYTE[]>(requiredSize);
        auto* detailData = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(detailBuffer.get());
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        SP_DEVINFO_DATA deviceInfoData = {};
        deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        if (!SetupDiGetDeviceInterfaceDetail(devInfo.get(), &deviceInterfaceData, detailData, requiredSize, nullptr, &deviceInfoData))
        {
            continue;
        }

        // Match to the filter name we are looking for
        if (_wcsicmp(detailData->DevicePath, filterName) == 0)
        {
            WCHAR instanceId[MAX_DEVICE_ID_LEN] = {};
            if (SetupDiGetDeviceInstanceId(devInfo.get(), &deviceInfoData, instanceId, ARRAYSIZE(instanceId), nullptr))
            {
                instanceIdOut = instanceId;
                return S_OK;
            }
            else
            {
                return HRESULT_FROM_WIN32(GetLastError());
            }
        }
    }

    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}

bool
SetDeviceEnabled
(
    _In_ PCWSTR deviceInstanceId,
    _In_ bool enable
)
{
    bool result = false;

    auto devInfo = unique_hdevinfo{SetupDiCreateDeviceInfoList(nullptr, nullptr)};
    if (!devInfo)
    {
        return false;
    }

    if (SetupDiOpenDeviceInfo(devInfo.get(), deviceInstanceId, nullptr, 0, NULL))
    {
        SP_DEVINFO_DATA devInfoData = {};
        devInfoData.cbSize = sizeof(devInfoData);
        if (SetupDiEnumDeviceInfo(devInfo.get(), 0, &devInfoData))
        {
            SP_PROPCHANGE_PARAMS propChangeParams = {};
            propChangeParams.ClassInstallHeader.cbSize = sizeof(propChangeParams.ClassInstallHeader);
            propChangeParams.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
            propChangeParams.Scope = DICS_FLAG_CONFIGSPECIFIC;
            propChangeParams.StateChange = enable ? DICS_ENABLE : DICS_DISABLE;

            auto cleanupOnFailure = wil::scope_exit([&]() {
                // if request was to disable and we failed, restore to enabled state
                // so it isn't applied after the next reboot, interfering with other tests.
                if (false == enable)
                {
                    propChangeParams.StateChange = DICS_ENABLE;
                    SetupDiSetClassInstallParams(devInfo.get(), &devInfoData, &propChangeParams.ClassInstallHeader, sizeof(propChangeParams));
                    SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, devInfo.get(), &devInfoData);
                }
            });

            if (SetupDiSetClassInstallParams(devInfo.get(), &devInfoData, &propChangeParams.ClassInstallHeader, sizeof(propChangeParams)))
            {
                if (SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, devInfo.get(), &devInfoData))
                {
                    result = true;
                    cleanupOnFailure.release();

                    // Give the driver state a chance to stabilize
                    Sleep(3000);
                }
            }
        }
    }

    return result;
}

