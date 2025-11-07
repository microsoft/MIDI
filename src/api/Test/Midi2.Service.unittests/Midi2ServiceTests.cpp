// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"

#include <Devpkey.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include "MidiTestCommon.h"
#include "WindowsMidiServices.h"

#include <initguid.h>
#include "MidiDefs.h"
#include "MidiXProc.h"
#include "Midi2ServiceTests.h"
#include "MidiSwEnum.h"

#include "Midi2MidiSrvTransport.h"
#include "Midi2KSTransport.h"

#define TEST_APPID {0xc24cc593, 0xbc6b, 0x4726,{ 0xb5, 0x52, 0xbe, 0xc8, 0x2d, 0xed, 0xb6, 0x8c}}

_Use_decl_annotations_
void __RPC_FAR* __RPC_API midl_user_allocate(size_t size)
{
    return (void*)new (std::nothrow) BYTE[size];
}

_Use_decl_annotations_
void __RPC_USER midl_user_free(void __RPC_FAR* p)
{
    delete[] (BYTE*)p;
}

using namespace winrt::Windows::Devices::Enumeration;

HRESULT
GetEndpointNativeDataFormat(_In_ std::wstring midiDevice, _Inout_ BYTE& nativeDataFormat)
{
    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(STRING_PKEY_MIDI_NativeDataFormat);
    auto deviceInfo = DeviceInformation::CreateFromIdAsync(midiDevice, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    auto prop = deviceInfo.Properties().Lookup(STRING_PKEY_MIDI_NativeDataFormat);
    if (prop)
    {
        try
        {
            // If a group index is provided by this device, it must be valid.
            nativeDataFormat = (BYTE) winrt::unbox_value<uint8_t>(prop);

            // minmidi doesn't specify a native data format, for the purposes of this test it's UMP.
            if (nativeDataFormat == 0)
            {
                nativeDataFormat = MidiDataFormats_UMP;
            }

            RETURN_HR_IF(E_UNEXPECTED, nativeDataFormat != MidiDataFormats_ByteStream && nativeDataFormat != MidiDataFormats_UMP);
        }
        CATCH_LOG();
    }

    return S_OK;
}

// using the protocol and endpoint, retrieve the midisrv
// rpc binding handle
HRESULT GetMidiSrvBindingHandle(handle_t* bindingHandle)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == bindingHandle);
    *bindingHandle = NULL;

    RPC_WSTR stringBinding = nullptr;
    auto cleanupOnExit = wil::scope_exit([&]() {
        if (stringBinding)
        {
            RpcStringFree(&stringBinding);
        }
    });

    RETURN_IF_WIN32_ERROR(RpcStringBindingCompose(
        nullptr,
        (RPC_WSTR)MIDISRV_LRPC_PROTOCOL,
        nullptr,
        nullptr,
        nullptr,
        &stringBinding));

    RETURN_IF_WIN32_ERROR(RpcBindingFromStringBinding(
        stringBinding,
        bindingHandle));

    return S_OK;
}

void CleanupClient(_In_ PMIDISRV_CLIENT *client)
{
    if (*client)
    {
        SAFE_CLOSEHANDLE((*client)->MidiInDataFileMapping);
        SAFE_CLOSEHANDLE((*client)->MidiInRegisterFileMapping);
        SAFE_CLOSEHANDLE((*client)->MidiInWriteEvent);
        SAFE_CLOSEHANDLE((*client)->MidiInReadEvent);
        SAFE_CLOSEHANDLE((*client)->MidiOutDataFileMapping);
        SAFE_CLOSEHANDLE((*client)->MidiOutRegisterFileMapping);
        SAFE_CLOSEHANDLE((*client)->MidiOutWriteEvent);
        SAFE_CLOSEHANDLE((*client)->MidiOutReadEvent);
    
        MIDL_user_free(*client);
        *client = nullptr;
    }
}

void Midi2ServiceTests::TestMidiServiceClientRPC()
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    MIDISRV_CLIENTCREATION_PARAMS creationParams { };
    PMIDISRV_CLIENT client {nullptr};
    wil::unique_rpc_binding bindingHandle;
    MidiClientHandle clientHandle{ 0 };
    std::unique_ptr<CMidiXProc> midiPump;
    DWORD MmCssTaskId{ 0 };

    // create the client session on the service before calling EnumerateDevices, which will kickstart
    // the service if it's not already running.
    LOG_OUTPUT(L"Retrieving binding handle");
    VERIFY_SUCCEEDED(GetMidiSrvBindingHandle(&bindingHandle));

    GUID dummySessionId{};
    PVOID contextHandle{ nullptr };

    VERIFY_SUCCEEDED([&](){
        // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
        // with structured exception handling.
        RpcTryExcept RETURN_IF_FAILED(MidiSrvRegisterSession(bindingHandle.get(), dummySessionId, L"ServiceTest", &contextHandle));
        RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
        RpcEndExcept
        return S_OK;
    }());

    LOG_OUTPUT(L"Enumerating devices");

    std::vector<std::unique_ptr<MIDIU_DEVICE>> testDevices;
    VERIFY_SUCCEEDED(MidiSWDeviceEnum::EnumerateDevices(testDevices, [&](PMIDIU_DEVICE device)
    {
        if (device->Flow == MidiFlowBidirectional &&
            (std::wstring::npos != device->ParentDeviceInstanceId.find(L"MINMIDI") ||
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"VID_CAFE&PID_4001&MI_02") ||
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"VID_0582&PID_0168&MI_00")) &&
            !device->MidiOne)
        {
            return true;
        }
        else
        {
            return false;
        }
    }));

    if (testDevices.size() == 0)
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Test requires at least 1 MinMidi bidi endpoint.");
        return;
    }

    VERIFY_IS_TRUE(testDevices.size() > 0);

    std::wstring midiDevice = testDevices[0]->DeviceId;
    
    auto cleanupOnExit = wil::scope_exit([&]() {

        if (midiPump)
        {
            midiPump->Shutdown();
        }

        CleanupClient(&client);

        if (0 != clientHandle)
        {
            HRESULT hr = ([&]()
            {
                // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
                // with structured exception handling.
                RpcTryExcept RETURN_IF_FAILED(MidiSrvDestroyClient(bindingHandle.get(), clientHandle));
                RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
                RpcEndExcept
                    return S_OK;
            }());

            if (FAILED(hr))
            {
                LOG_OUTPUT(L"MidiSrvDestroyClient failed 0x%08x", hr);
            }
        }
    });

    BYTE nativeDataFormat {0};
    VERIFY_SUCCEEDED(GetEndpointNativeDataFormat(midiDevice.c_str(), nativeDataFormat));

    creationParams.DataFormat = MidiDataFormats_UMP;
    creationParams.Flow = MidiFlowBidirectional;
    creationParams.BufferSize = PAGE_SIZE;

    VERIFY_SUCCEEDED([&]() {
        // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
        // with structured exception handling.
        RpcTryExcept RETURN_IF_FAILED(MidiSrvCreateClient(bindingHandle.get(), midiDevice.c_str(), &creationParams, dummySessionId, &client));
        RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
        RpcEndExcept
        return S_OK;
    }());

    clientHandle = client->ClientHandle;

    std::unique_ptr<MEMORY_MAPPED_PIPE> midiInPipe = std::unique_ptr<MEMORY_MAPPED_PIPE>(new (std::nothrow) MEMORY_MAPPED_PIPE);
    VERIFY_IS_TRUE(nullptr != midiInPipe);

    std::unique_ptr <MEMORY_MAPPED_PIPE> midiOutPipe = std::unique_ptr<MEMORY_MAPPED_PIPE>(new (std::nothrow) MEMORY_MAPPED_PIPE);
    VERIFY_IS_TRUE(nullptr != midiOutPipe);

    midiInPipe->DataFormat = creationParams.DataFormat;

    midiInPipe->DataBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
    VERIFY_IS_TRUE(nullptr != midiInPipe->DataBuffer);

    midiInPipe->RegistersBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
    VERIFY_IS_TRUE(nullptr != midiInPipe->RegistersBuffer);

    midiOutPipe->DataFormat = creationParams.DataFormat;

    midiOutPipe->DataBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
    VERIFY_IS_TRUE(nullptr != midiOutPipe->DataBuffer);

    midiOutPipe->RegistersBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
    VERIFY_IS_TRUE(nullptr != midiOutPipe->RegistersBuffer);

    // Transfer the handles to local storage
    midiInPipe->DataBuffer->FileMapping.reset(client->MidiInDataFileMapping);
    client->MidiInDataFileMapping = NULL;
    midiInPipe->RegistersBuffer->FileMapping.reset(client->MidiInRegisterFileMapping);
    client->MidiInRegisterFileMapping = NULL;
    midiInPipe->WriteEvent.reset(client->MidiInWriteEvent);
    client->MidiInWriteEvent = NULL;
    midiInPipe->ReadEvent.reset(client->MidiInReadEvent);
    client->MidiInReadEvent = NULL;
    midiInPipe->Data.BufferSize = client->MidiInBufferSize;
    midiOutPipe->DataBuffer->FileMapping.reset(client->MidiOutDataFileMapping);
    client->MidiOutDataFileMapping = NULL;
    midiOutPipe->RegistersBuffer->FileMapping.reset(client->MidiOutRegisterFileMapping);
    client->MidiOutRegisterFileMapping = NULL;
    midiOutPipe->WriteEvent.reset(client->MidiOutWriteEvent);
    client->MidiOutWriteEvent = NULL;
    midiOutPipe->ReadEvent.reset(client->MidiOutReadEvent);
    client->MidiOutReadEvent = NULL;
    midiOutPipe->Data.BufferSize = client->MidiOutBufferSize;

    MIDL_user_free(client);
    client = nullptr;

    // Midi in controls, buffering, and eventing.
    VERIFY_SUCCEEDED(CreateMappedDataBuffer(0, midiInPipe->DataBuffer.get(), &midiInPipe->Data));
    VERIFY_SUCCEEDED(CreateMappedRegisters(midiInPipe->RegistersBuffer.get(), &midiInPipe->Registers));

    // Midi out controls, buffering, and eventing
    VERIFY_SUCCEEDED(CreateMappedDataBuffer(0, midiOutPipe->DataBuffer.get(), &midiOutPipe->Data));
    VERIFY_SUCCEEDED(CreateMappedRegisters(midiOutPipe->RegistersBuffer.get(), &midiOutPipe->Registers));

    UINT midiMessagesReceived = 0;
    UINT messagesExpected{ 0 };
    wil::unique_event_nothrow allMessagesReceived;

    VERIFY_SUCCEEDED(allMessagesReceived.create());

    m_MidiInCallback = [&](PVOID payload, UINT32 payloadSize, LONGLONG payloadPosition, LONGLONG)
    {
        PrintMidiMessage(payload, payloadSize, sizeof(UMP32), payloadPosition);

        midiMessagesReceived++;
        if (midiMessagesReceived == messagesExpected)
        {
            allMessagesReceived.SetEvent();
        }
    };

    midiPump.reset(new (std::nothrow) CMidiXProc());
    VERIFY_IS_TRUE(nullptr != midiPump);

    VERIFY_SUCCEEDED(midiPump->Initialize(&MmCssTaskId, midiInPipe, midiOutPipe, this, 0, true));

    LOG_OUTPUT(L"Writing midi data");
    messagesExpected = 4;
    VERIFY_SUCCEEDED(midiPump->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData_32, sizeof(UMP32), 0));

    if (nativeDataFormat == MidiDataFormats_ByteStream)
    {
        VERIFY_SUCCEEDED(midiPump->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData_32, sizeof(UMP32), 0));
        VERIFY_SUCCEEDED(midiPump->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData_32, sizeof(UMP32), 0));
        VERIFY_SUCCEEDED(midiPump->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData_32, sizeof(UMP32), 0));
    }
    else
    {
        VERIFY_SUCCEEDED(midiPump->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData_64, sizeof(UMP64), 0));
        VERIFY_SUCCEEDED(midiPump->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData_96, sizeof(UMP96), 0));
        VERIFY_SUCCEEDED(midiPump->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData_128, sizeof(UMP128), 0));
    }

    VERIFY_IS_TRUE(allMessagesReceived.wait(5000));

}

void Midi2ServiceTests::TestMidiServiceInvalidCreationParams()
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    wil::unique_rpc_binding bindingHandle;

    // create the client session on the service before calling EnumerateDevices, which will kickstart
    // the service if it's not already running.
    LOG_OUTPUT(L"Retrieving binding handle");
    VERIFY_SUCCEEDED(GetMidiSrvBindingHandle(&bindingHandle));

    GUID dummySessionId{};
    PVOID contextHandle{ nullptr };

    VERIFY_SUCCEEDED(CoCreateGuid(&dummySessionId));

    VERIFY_SUCCEEDED([&](){
        // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
        // with structured exception handling.
        RpcTryExcept RETURN_IF_FAILED(MidiSrvRegisterSession(bindingHandle.get(), dummySessionId, L"TestMidiServiceBufferSizes", &contextHandle));
        RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
        RpcEndExcept
        return S_OK;
    }());

    LOG_OUTPUT(L"Enumerating devices");

    std::vector<std::unique_ptr<MIDIU_DEVICE>> testDevices;
    VERIFY_SUCCEEDED(MidiSWDeviceEnum::EnumerateDevices(testDevices, [&](PMIDIU_DEVICE device)
    {
        if (device->Flow == MidiFlowBidirectional &&
            (std::wstring::npos != device->ParentDeviceInstanceId.find(L"MINMIDI") ||
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"VID_CAFE&PID_4001&MI_02") ||
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"VID_0582&PID_0168&MI_00")) &&
            !device->MidiOne)
        {
            return true;
        }
        else
        {
            return false;
        }
    }));

    if (testDevices.size() == 0)
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Test requires at least 1 MinMidi bidi endpoint.");
        return;
    }

    VERIFY_IS_TRUE(testDevices.size() > 0);

    std::wstring midiDevice = testDevices[0]->DeviceId;
    PMIDISRV_CLIENT client {nullptr};

    MIDISRV_CLIENTCREATION_PARAMS creationParams[] = {
            { MessageOptionFlags_None, MidiDataFormats_UMP, MidiFlowBidirectional, 0 },
            { MessageOptionFlags_None, MidiDataFormats_UMP, MidiFlowBidirectional, MAXIMUM_LOOPED_BUFFER_SIZE + 1 },
            { MessageOptionFlags_None, MidiDataFormats_UMP, MidiFlowBidirectional, MAXIMUM_LOOPED_BUFFER_SIZE + 2 },
            { MessageOptionFlags_None, MidiDataFormats_UMP, MidiFlowBidirectional, MAXIMUM_LOOPED_BUFFER_SIZE + PAGE_SIZE },
            { MessageOptionFlags_None, MidiDataFormats_UMP, MidiFlowBidirectional, ULONG_MAX },
            { MessageOptionFlags_None, MidiDataFormats_UMP, MidiFlowBidirectional, ULONG_MAX - 1 },
            { MessageOptionFlags_None, MidiDataFormats_UMP, MidiFlowBidirectional, ULONG_MAX - 2 },
            { MessageOptionFlags_None, MidiDataFormats_UMP, MidiFlowBidirectional, ULONG_MAX - PAGE_SIZE },
            { MessageOptionFlags_None, MidiDataFormats_UMP, MidiFlowBidirectional, ULONG_MAX - PAGE_SIZE - 1 },
            { MessageOptionFlags_None, MidiDataFormats_UMP, MidiFlowBidirectional, ULONG_MAX - PAGE_SIZE - 2 },
            { MessageOptionFlags_None, MidiDataFormats_UMP, MidiFlowBidirectional, ULONG_MAX/2 + 1 },
            { MessageOptionFlags_None, (MidiDataFormats) 0, MidiFlowBidirectional, PAGE_SIZE },
            { MessageOptionFlags_None, (MidiDataFormats) 4, MidiFlowBidirectional, PAGE_SIZE },  // 1, 2, and 3 are valid formats
            { MessageOptionFlags_None, (MidiDataFormats) (MidiDataFormats_Any - 1), MidiFlowBidirectional, PAGE_SIZE },  // 1, 2, and 3 are valid formats
            { MessageOptionFlags_None, MidiDataFormats_UMP, (MidiFlow) (MidiFlowBidirectional + 1), PAGE_SIZE },
            { MessageOptionFlags_None, MidiDataFormats_UMP, (MidiFlow) (MidiFlowBidirectional + 2), PAGE_SIZE },
        };

    // In the event that a creation does succeed, clean it up on our way out.
    auto cleanupOnExit = wil::scope_exit([&]() {
        CleanupClient(&client);
    });

    for (UINT i = 0; i < _countof(creationParams); i++)
    {
        LOG_OUTPUT(L"Testing index %d, data format %d, flow %d, buffer size %lu", i, creationParams[i].DataFormat, creationParams[i].Flow, creationParams[i].BufferSize);

        VERIFY_FAILED([&]() {
            // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
            // with structured exception handling.
            RpcTryExcept RETURN_IF_FAILED(MidiSrvCreateClient(bindingHandle.get(), midiDevice.c_str(), &creationParams[i], dummySessionId, &client));
            RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
            RpcEndExcept
            return S_OK;
        }());
    }

    MIDISRV_CLIENTCREATION_PARAMS creationParam = { MessageOptionFlags_None, MidiDataFormats_UMP, MidiFlowBidirectional, PAGE_SIZE };

    LOG_OUTPUT(L"Testing invalid binding handle");
    VERIFY_FAILED([&]() {
        // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
        // with structured exception handling.
        RpcTryExcept RETURN_IF_FAILED(MidiSrvCreateClient(NULL, midiDevice.c_str(), &creationParam, dummySessionId, &client));
        RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
        RpcEndExcept
        return S_OK;
    }());

    LOG_OUTPUT(L"Testing invalid device");
    VERIFY_FAILED([&]() {
        // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
        // with structured exception handling.
        RpcTryExcept RETURN_IF_FAILED(MidiSrvCreateClient(bindingHandle.get(), nullptr, &creationParam, dummySessionId, &client));
        RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
        RpcEndExcept
        return S_OK;
    }());

    LOG_OUTPUT(L"Testing invalid creation params");
    VERIFY_FAILED([&]() {
        // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
        // with structured exception handling.
        RpcTryExcept RETURN_IF_FAILED(MidiSrvCreateClient(bindingHandle.get(), midiDevice.c_str(), nullptr, dummySessionId, &client));
        RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
        RpcEndExcept
        return S_OK;
    }());

    LOG_OUTPUT(L"Testing invalid client");
    VERIFY_FAILED([&]() {
        // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
        // with structured exception handling.
        RpcTryExcept RETURN_IF_FAILED(MidiSrvCreateClient(bindingHandle.get(), midiDevice.c_str(), &creationParam, dummySessionId, nullptr));
        RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
        RpcEndExcept
        return S_OK;
    }());

}

void Midi2ServiceTests::TestMidiServiceFailedCreation()
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    wil::unique_rpc_binding bindingHandle;

    // create the client session on the service before calling EnumerateDevices, which will kickstart
    // the service if it's not already running.
    LOG_OUTPUT(L"Retrieving binding handle");
    VERIFY_SUCCEEDED(GetMidiSrvBindingHandle(&bindingHandle));

    GUID dummySessionId{};
    PVOID contextHandle{ nullptr };
    DWORD MmCssTaskId{ 0 };
    TRANSPORTCREATIONPARAMS transportCreationParams { MessageOptionFlags_None, MidiDataFormats_UMP, TEST_APPID };
    wil::com_ptr_nothrow<IMidiTransport> midiTransport;
    wil::com_ptr_nothrow<IMidiBidirectional> midiKSDevice;
    std::vector<std::unique_ptr<MIDIU_DEVICE>> testDevices;

    VERIFY_SUCCEEDED(CoCreateGuid(&dummySessionId));
    VERIFY_SUCCEEDED([&](){
        // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
        // with structured exception handling.
        RpcTryExcept RETURN_IF_FAILED(MidiSrvRegisterSession(bindingHandle.get(), dummySessionId, L"TestMidiServiceFailedCreation", &contextHandle));
        RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
        RpcEndExcept
        return S_OK;
    }());

    // This test has two parts, part 1 is locating a KS device that isn't currently in use and opening the KS pin on it.
    // Part 2 is attempting to use that same device from within the service, which will result in a late-failure during
    // initialization, triggering additional cleanup codepaths.
    LOG_OUTPUT(L"Enumerating devices, locating a KS device that is not currently in use.");

    VERIFY_SUCCEEDED(CoCreateInstance(__uuidof(Midi2KSTransport), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiTransport)));
    VERIFY_SUCCEEDED(MidiSWDeviceEnum::EnumerateDevices(testDevices, [&](PMIDIU_DEVICE device)
    {
        if (!midiKSDevice &&
            device->Flow == MidiFlowBidirectional &&
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"MINMIDI") &&
            !device->MidiOne)
        {
            wil::com_ptr_nothrow<IMidiBidirectional> testDevice;
            VERIFY_SUCCEEDED(midiTransport->Activate(__uuidof(IMidiBidirectional), (void **) &testDevice));
            if (SUCCEEDED(testDevice->Initialize(device->DeviceId.c_str(), &transportCreationParams, &MmCssTaskId, this, 0, dummySessionId)))
            {
                midiKSDevice = testDevice;
                return true;
            }
        }

        return false;
    }));

    if (testDevices.size() == 0 || !midiKSDevice)
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Test requires at least 1 MinMidi bidi endpoint.");
        return;
    }

    VERIFY_IS_TRUE(testDevices.size() > 0);

    std::wstring midiDevice = testDevices[0]->DeviceId;
    PMIDISRV_CLIENT client {nullptr};

    // In the event that a creation does succeed, clean it up on our way out.
    auto cleanupOnExit = wil::scope_exit([&]() {
        CleanupClient(&client);
    });

    MIDISRV_CLIENTCREATION_PARAMS creationParam = { MessageOptionFlags_None, MidiDataFormats_UMP, MidiFlowBidirectional, PAGE_SIZE };

    LOG_OUTPUT(L"Testing creation with driver in use");
    VERIFY_FAILED([&]() {
        // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
        // with structured exception handling.
        RpcTryExcept RETURN_IF_FAILED(MidiSrvCreateClient(bindingHandle.get(), midiDevice.c_str(), &creationParam, dummySessionId, &client));
        RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
        RpcEndExcept
        return S_OK;
    }());

}

bool Midi2ServiceTests::TestSetup()
{
    m_MidiInCallback = nullptr;
    return true;
}

bool Midi2ServiceTests::TestCleanup()
{
    return true;
}

bool Midi2ServiceTests::ClassSetup()
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    return true;
}

bool Midi2ServiceTests::ClassCleanup()
{
    return true;
}

