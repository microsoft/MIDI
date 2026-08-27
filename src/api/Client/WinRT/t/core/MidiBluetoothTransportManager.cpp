// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "MidiBluetoothTransportManager.h"
#include "Transports.Bluetooth.MidiBluetoothTransportManager.g.cpp"

#include "MidiBluetoothDeviceInformation.h"
#include "MidiBluetoothDeviceConnectResponse.h"
#include "MidiBluetoothDeviceDisconnectResponse.h"
#include "MidiBluetoothPeripheralResponse.h"
#include "MidiBluetoothPeripheralStatus.h"
#include "MidiBluetoothRadioInformation.h"

#include "MidiReporting.h"
#include "MidiServiceConfigResponse.h"
#include "MidiServiceTransportCommand.h"
#include "MidiServiceTransportPluginConfigManager.h"

#include "midi_bluetooth_utility.h"

namespace btinternal = ::Windows::Devices::Midi2::Transports::Bluetooth::Internal;

namespace
{
    struct CommandOutcome
    {
        bool Success{ false };
        winrt::hstring ErrorMessage{};
        uint32_t ServiceErrorCode{ 0 };
        int32_t ErrorHResult{ 0 };
        json::JsonObject ResponseJson{ nullptr };
    };

    CommandOutcome SendTransportCommand(
        _In_ winrt::hstring const& verb,
        _In_ std::map<std::wstring, std::wstring> const& arguments) noexcept
    {
        CommandOutcome outcome{};

        try
        {
            svc::MidiServiceTransportCommand command{
                winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation::MidiBluetoothTransportManager::TransportId(),
                verb };

            for (auto const& argument : arguments)
            {
                command.Arguments().Insert(argument.first, argument.second);
            }

            auto const response = svc::MidiServiceTransportPluginConfigManager::SendCommand(command);

            if (response == nullptr)
            {
                outcome.ServiceErrorCode = BLUETOOTH_MIDI_ERROR_CODE_TRANSPORT_NOT_AVAILABLE;
                outcome.ErrorMessage = internal::ResourceGetHString(IDS_BLUETOOTH_ERROR_NO_SERVICE_RESPONSE);
                return outcome;
            }

            outcome.ResponseJson = response.ResponseJson();
            outcome.Success = response.Status() == svc::MidiServiceConfigResponseStatus::Success;
            outcome.ErrorMessage = response.ServiceErrorMessage();
            outcome.ServiceErrorCode = response.ServiceErrorCode();

            if (outcome.ResponseJson != nullptr)
            {
                outcome.ErrorHResult = static_cast<int32_t>(
                    outcome.ResponseJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_RESULT_HRESULT_KEY, 0.0));
            }
        }
        catch (...)
        {
            outcome.Success = false;
            outcome.ServiceErrorCode = BLUETOOTH_MIDI_ERROR_CODE_TRANSPORT_NOT_AVAILABLE;
            outcome.ErrorMessage = internal::ResourceGetHString(IDS_BLUETOOTH_ERROR_NO_SERVICE_RESPONSE);
        }

        return outcome;
    }

    bluetooth::MidiBluetoothPeripheralStatus BuildPeripheralStatus(_In_ json::JsonObject const& responseJson) noexcept
    {
        auto status = winrt::make_self<winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation::MidiBluetoothPeripheralStatus>();

        if (responseJson != nullptr)
        {
            json::JsonObject peripheralJson{ nullptr };

            if (responseJson.TryLookup(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_KEY) != nullptr)
            {
                peripheralJson = responseJson.GetNamedObject(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_KEY, nullptr);
            }

            status->InternalInitializeFromJson(peripheralJson);
        }

        return *status;
    }
}

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    bool MidiBluetoothTransportManager::IsTransportAvailable() noexcept
    {
        try
        {
            auto transports = rpt::MidiReporting::GetInstalledTransportPlugins();

            for (auto const& transport : transports)
            {
                if (transport.TransportId() == TransportId())
                {
                    return true;
                }
            }

            return false;
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error checking Bluetooth transport availability.");
            return false;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception checking Bluetooth transport availability.");
            return false;
        }
    }

    collections::IVectorView<bluetooth::MidiBluetoothDeviceInformation> MidiBluetoothTransportManager::GetAvailableDevices() noexcept
    {
        auto devices = winrt::single_threaded_vector<bluetooth::MidiBluetoothDeviceInformation>();

        try
        {
            auto const outcome = SendTransportCommand(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_LIST_AVAILABLE_DEVICES, {});

            if (!outcome.Success || outcome.ResponseJson == nullptr)
            {
                return devices.GetView();
            }

            auto const devicesJson = outcome.ResponseJson.GetNamedArray(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_AVAILABLE_DEVICES_RESPONSE_KEY, nullptr);

            if (devicesJson == nullptr)
            {
                return devices.GetView();
            }

            for (uint32_t i = 0; i < devicesJson.Size(); i++)
            {
                auto const entry = devicesJson.GetObjectAt(i);

                if (entry == nullptr)
                {
                    continue;
                }

                auto device = winrt::make_self<implementation::MidiBluetoothDeviceInformation>();
                device->InternalInitializeFromJson(entry);

                devices.Append(*device);
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception listing Bluetooth MIDI devices.");
        }

        return devices.GetView();
    }

    _Use_decl_annotations_
    bluetooth::MidiBluetoothDeviceInformation MidiBluetoothTransportManager::GetDevice(
        winrt::hstring const& bluetoothDeviceId) noexcept
    {
        try
        {
            if (internal::TrimmedHStringCopy(bluetoothDeviceId).empty())
            {
                return nullptr;
            }

            for (auto const& device : GetAvailableDevices())
            {
                if (internal::ToUpperTrimmedWStringCopy(std::wstring{ device.BluetoothDeviceId() }) ==
                    internal::ToUpperTrimmedWStringCopy(std::wstring{ bluetoothDeviceId }))
                {
                    return device;
                }
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception looking up a Bluetooth MIDI device.");
        }

        return nullptr;
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<bluetooth::MidiBluetoothDeviceConnectResponse> MidiBluetoothTransportManager::ConnectDeviceAsync(
        bluetooth::MidiBluetoothDeviceConnectConfig connectConfig) noexcept
    {
        auto result = winrt::make_self<implementation::MidiBluetoothDeviceConnectResponse>();

        if (connectConfig == nullptr || internal::TrimmedHStringCopy(connectConfig.BluetoothDeviceId()).empty())
        {
            result->InternalInitialize(
                false,
                bluetooth::MidiBluetoothDeviceConnectErrorCode::InvalidBluetoothDeviceId,
                internal::ResourceGetHString(IDS_BLUETOOTH_ERROR_MISSING_DEVICE_ID),
                E_INVALIDARG,
                false,
                nullptr);

            co_return *result;
        }

        auto const bluetoothDeviceId = connectConfig.BluetoothDeviceId();

        co_await winrt::resume_background();

        try
        {
            auto const outcome = SendTransportCommand(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_CONNECT_DEVICE,
                { { MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_ARGUMENT_DEVICE_ID_KEY, std::wstring{ bluetoothDeviceId } } });

            bool isKnown{ false };

            if (outcome.ResponseJson != nullptr)
            {
                isKnown = outcome.ResponseJson.GetNamedBoolean(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_KNOWN_KEY, false);
            }

            result->InternalInitialize(
                outcome.Success,
                outcome.Success
                    ? bluetooth::MidiBluetoothDeviceConnectErrorCode::Success
                    : static_cast<bluetooth::MidiBluetoothDeviceConnectErrorCode>(outcome.ServiceErrorCode),
                outcome.ErrorMessage,
                outcome.ErrorHResult,
                isKnown,
                outcome.Success ? GetDevice(bluetoothDeviceId) : nullptr);
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception connecting a Bluetooth MIDI device.");

            result->InternalInitialize(
                false,
                bluetooth::MidiBluetoothDeviceConnectErrorCode::Unexpected,
                internal::ResourceGetHString(IDS_BLUETOOTH_ERROR_NO_SERVICE_RESPONSE),
                0,
                false,
                nullptr);
        }

        co_return *result;
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<bluetooth::MidiBluetoothDeviceDisconnectResponse> MidiBluetoothTransportManager::DisconnectDeviceAsync(
        bluetooth::MidiBluetoothDeviceDisconnectConfig disconnectConfig) noexcept
    {
        auto result = winrt::make_self<implementation::MidiBluetoothDeviceDisconnectResponse>();

        if (disconnectConfig == nullptr || internal::TrimmedHStringCopy(disconnectConfig.BluetoothDeviceId()).empty())
        {
            result->InternalInitialize(
                false,
                bluetooth::MidiBluetoothDeviceDisconnectErrorCode::InvalidBluetoothDeviceId,
                internal::ResourceGetHString(IDS_BLUETOOTH_ERROR_MISSING_DEVICE_ID),
                E_INVALIDARG);

            co_return *result;
        }

        auto const bluetoothDeviceId = disconnectConfig.BluetoothDeviceId();

        co_await winrt::resume_background();

        try
        {
            auto const outcome = SendTransportCommand(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_DISCONNECT_DEVICE,
                { { MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_ARGUMENT_DEVICE_ID_KEY, std::wstring{ bluetoothDeviceId } } });

            result->InternalInitialize(
                outcome.Success,
                outcome.Success
                    ? bluetooth::MidiBluetoothDeviceDisconnectErrorCode::Success
                    : static_cast<bluetooth::MidiBluetoothDeviceDisconnectErrorCode>(outcome.ServiceErrorCode),
                outcome.ErrorMessage,
                outcome.ErrorHResult);
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception disconnecting a Bluetooth MIDI device.");

            result->InternalInitialize(
                false,
                bluetooth::MidiBluetoothDeviceDisconnectErrorCode::Unexpected,
                internal::ResourceGetHString(IDS_BLUETOOTH_ERROR_NO_SERVICE_RESPONSE),
                0);
        }

        co_return *result;
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<bluetooth::MidiBluetoothPeripheralResponse> MidiBluetoothTransportManager::StartPeripheralAsync(
        bluetooth::MidiBluetoothPeripheralConfig peripheralConfig) noexcept
    {
        auto result = winrt::make_self<implementation::MidiBluetoothPeripheralResponse>();

        auto const protocol = peripheralConfig != nullptr
            ? peripheralConfig.Protocol()
            : bluetooth::MidiBluetoothProtocol::Unknown;

        if (protocol == bluetooth::MidiBluetoothProtocol::Unknown)
        {
            result->InternalInitialize(
                false,
                bluetooth::MidiBluetoothPeripheralErrorCode::InvalidProtocol,
                internal::ResourceGetHString(IDS_BLUETOOTH_ERROR_PERIPHERAL_PROTOCOL_REQUIRED),
                E_INVALIDARG,
                nullptr);

            co_return *result;
        }

        co_await winrt::resume_background();

        try
        {
            auto const outcome = SendTransportCommand(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_START_PERIPHERAL,
                { { MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_PROTOCOL_KEY,
                    std::wstring{ btinternal::ProtocolToJsonString(protocol) } } });

            result->InternalInitialize(
                outcome.Success,
                outcome.Success
                    ? bluetooth::MidiBluetoothPeripheralErrorCode::Success
                    : static_cast<bluetooth::MidiBluetoothPeripheralErrorCode>(outcome.ServiceErrorCode),
                outcome.ErrorMessage,
                outcome.ErrorHResult,
                BuildPeripheralStatus(outcome.ResponseJson));
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception starting the Bluetooth MIDI peripheral.");

            result->InternalInitialize(
                false,
                bluetooth::MidiBluetoothPeripheralErrorCode::Unexpected,
                internal::ResourceGetHString(IDS_BLUETOOTH_ERROR_NO_SERVICE_RESPONSE),
                0,
                nullptr);
        }

        co_return *result;
    }

    foundation::IAsyncOperation<bluetooth::MidiBluetoothPeripheralResponse> MidiBluetoothTransportManager::StopPeripheralAsync() noexcept
    {
        co_await winrt::resume_background();

        auto result = winrt::make_self<implementation::MidiBluetoothPeripheralResponse>();

        try
        {
            auto const outcome = SendTransportCommand(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_STOP_PERIPHERAL, {});

            result->InternalInitialize(
                outcome.Success,
                outcome.Success
                    ? bluetooth::MidiBluetoothPeripheralErrorCode::Success
                    : static_cast<bluetooth::MidiBluetoothPeripheralErrorCode>(outcome.ServiceErrorCode),
                outcome.ErrorMessage,
                outcome.ErrorHResult,
                BuildPeripheralStatus(outcome.ResponseJson));
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception stopping the Bluetooth MIDI peripheral.");

            result->InternalInitialize(
                false,
                bluetooth::MidiBluetoothPeripheralErrorCode::Unexpected,
                internal::ResourceGetHString(IDS_BLUETOOTH_ERROR_NO_SERVICE_RESPONSE),
                0,
                nullptr);
        }

        co_return *result;
    }

    bluetooth::MidiBluetoothPeripheralStatus MidiBluetoothTransportManager::GetPeripheralStatus() noexcept
    {
        try
        {
            auto const outcome = SendTransportCommand(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_GET_PERIPHERAL_STATUS, {});

            return BuildPeripheralStatus(outcome.ResponseJson);
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception reading Bluetooth MIDI peripheral status.");
        }

        return BuildPeripheralStatus(nullptr);
    }

    bluetooth::MidiBluetoothRadioInformation MidiBluetoothTransportManager::GetRadioInformation() noexcept
    {
        try
        {
            auto const outcome = SendTransportCommand(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_GET_PERIPHERAL_STATUS, {});

            if (outcome.ResponseJson == nullptr ||
                outcome.ResponseJson.TryLookup(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_RADIO_KEY) == nullptr)
            {
                // an older service does not report this, and saying "no radio" would be worse
                // than saying nothing
                return nullptr;
            }

            auto const radioJson = outcome.ResponseJson.GetNamedObject(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_RADIO_KEY, nullptr);

            if (radioJson == nullptr)
            {
                return nullptr;
            }

            auto radio = winrt::make_self<implementation::MidiBluetoothRadioInformation>();
            radio->InternalInitializeFromJson(radioJson);

            return *radio;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception reading Bluetooth radio information.");
        }

        return nullptr;
    }
}
