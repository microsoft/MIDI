// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <usbioctl.h>
#include <usbiodef.h>
#include <usbspec.h>

#include "MidiPnpUtilities.h"
#include "midi_ksa_usb_strings.h"

namespace KsaUsbStrings
{
    namespace
    {
        // A hub which does not answer must not hold up device enumeration. Each request is capped,
        // and the whole lookup is three requests at worst.
        constexpr DWORD UsbRequestTimeoutMilliseconds = 500;

        constexpr USHORT FallbackLanguageId = 0x0409;   // US English, when the device lists none

        constexpr size_t MaximumManufacturerNameCharacters = 128;

        // Completes, times out, or fails. On timeout the request is canceled and then waited on,
        // because the stack would otherwise still be free to write into the caller's buffer.
        bool DeviceIoControlWithTimeout(
            _In_ HANDLE deviceHandle,
            _In_ DWORD const controlCode,
            _In_reads_bytes_(inputSize) void* input,
            _In_ DWORD const inputSize,
            _Out_writes_bytes_to_(outputSize, bytesReturned) void* output,
            _In_ DWORD const outputSize,
            _Out_ DWORD& bytesReturned) noexcept
        {
            bytesReturned = 0;

            wil::unique_event_nothrow completed;

            if (FAILED(completed.create(wil::EventOptions::ManualReset)))
            {
                return false;
            }

            OVERLAPPED overlapped{ };
            overlapped.hEvent = completed.get();

            if (DeviceIoControl(deviceHandle, controlCode, input, inputSize, output, outputSize, &bytesReturned, &overlapped))
            {
                return true;
            }

            if (GetLastError() != ERROR_IO_PENDING)
            {
                return false;
            }

            if (WaitForSingleObject(completed.get(), UsbRequestTimeoutMilliseconds) != WAIT_OBJECT_0)
            {
                LOG_IF_WIN32_BOOL_FALSE(CancelIoEx(deviceHandle, &overlapped));

                DWORD cancelledBytes{ 0 };
                GetOverlappedResult(deviceHandle, &overlapped, &cancelledBytes, TRUE);

                return false;
            }

            return GetOverlappedResult(deviceHandle, &overlapped, &bytesReturned, FALSE) != FALSE;
        }

        bool TryGetHubPathAndPort(
            _In_ std::wstring const& usbDeviceInstanceId,
            _Inout_ std::wstring& hubInterfacePath,
            _Inout_ ULONG& portNumber) noexcept
        {
            DEVINST deviceInstance{ 0 };

            if (CM_Locate_DevNodeW(&deviceInstance, const_cast<DEVINSTID_W>(usbDeviceInstanceId.c_str()), CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS)
            {
                return false;
            }

            // the port the device sits on, which is how the hub identifies it
            DEVPROPTYPE propertyType{ 0 };
            ULONG size = sizeof(portNumber);

            if (CM_Get_DevNode_PropertyW(deviceInstance, &DEVPKEY_Device_Address, &propertyType, (PBYTE)&portNumber, &size, 0) != CR_SUCCESS)
            {
                return false;
            }

            if (propertyType != DEVPROP_TYPE_UINT32 || portNumber == 0)
            {
                return false;
            }

            DEVINST hubInstance{ 0 };

            if (CM_Get_Parent(&hubInstance, deviceInstance, 0) != CR_SUCCESS)
            {
                return false;
            }

            WCHAR hubInstanceId[MAX_DEVICE_ID_LEN]{ 0 };

            if (CM_Get_Device_IDW(hubInstance, hubInstanceId, ARRAYSIZE(hubInstanceId), 0) != CR_SUCCESS)
            {
                return false;
            }

            ULONG listSize{ 0 };

            if (CM_Get_Device_Interface_List_SizeW(
                &listSize,
                const_cast<LPGUID>(&GUID_DEVINTERFACE_USB_HUB),
                hubInstanceId,
                CM_GET_DEVICE_INTERFACE_LIST_PRESENT) != CR_SUCCESS)
            {
                return false;
            }

            if (listSize < 2 || listSize > 64 * 1024)
            {
                return false;
            }

            std::vector<WCHAR> list(listSize, L'\0');

            if (CM_Get_Device_Interface_ListW(
                const_cast<LPGUID>(&GUID_DEVINTERFACE_USB_HUB),
                hubInstanceId,
                list.data(),
                listSize,
                CM_GET_DEVICE_INTERFACE_LIST_PRESENT) != CR_SUCCESS)
            {
                return false;
            }

            // the list is double null terminated; the first entry is the one we want
            hubInterfacePath.assign(list.data());

            return !hubInterfacePath.empty();
        }

        // Descriptor contents come from the device, so nothing in here is trusted.
        std::wstring ReadStringDescriptor(
            _In_ HANDLE hubHandle,
            _In_ ULONG const portNumber,
            _In_ UCHAR const descriptorIndex,
            _In_ USHORT const languageId) noexcept
        {
            if (descriptorIndex == 0) { return {}; }

            constexpr size_t bufferSize = sizeof(USB_DESCRIPTOR_REQUEST) + MAXIMUM_USB_STRING_LENGTH;

            std::vector<uint8_t> buffer(bufferSize, 0);

            auto request = reinterpret_cast<USB_DESCRIPTOR_REQUEST*>(buffer.data());

            request->ConnectionIndex = portNumber;
            request->SetupPacket.bmRequest = 0x80;          // device to host, standard, device
            request->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
            request->SetupPacket.wValue = static_cast<USHORT>((USB_STRING_DESCRIPTOR_TYPE << 8) | descriptorIndex);
            request->SetupPacket.wIndex = languageId;
            request->SetupPacket.wLength = MAXIMUM_USB_STRING_LENGTH;

            DWORD bytesReturned{ 0 };

            if (!DeviceIoControlWithTimeout(
                hubHandle,
                IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
                buffer.data(),
                (DWORD)buffer.size(),
                buffer.data(),
                (DWORD)buffer.size(),
                bytesReturned))
            {
                return {};
            }

            if (bytesReturned <= sizeof(USB_DESCRIPTOR_REQUEST)) { return {}; }

            auto const available = bytesReturned - sizeof(USB_DESCRIPTOR_REQUEST);

            if (available < sizeof(USB_COMMON_DESCRIPTOR)) { return {}; }

            auto descriptor = reinterpret_cast<USB_STRING_DESCRIPTOR*>(request->Data);

            if (descriptor->bDescriptorType != USB_STRING_DESCRIPTOR_TYPE) { return {}; }
            if (descriptor->bLength < sizeof(USB_COMMON_DESCRIPTOR)) { return {}; }
            if ((descriptor->bLength % 2) != 0) { return {}; }

            // a device claiming more than the hub returned is the case that would read off the end
            if (descriptor->bLength > available) { return {}; }

            auto const characterCount = (descriptor->bLength - sizeof(USB_COMMON_DESCRIPTOR)) / sizeof(WCHAR);

            if (characterCount == 0 || characterCount > MAXIMUM_USB_STRING_LENGTH) { return {}; }

            std::wstring value(descriptor->bString, characterCount);

            // descriptors are not required to be null terminated, and some carry padding
            auto const firstNull = value.find(L'\0');
            if (firstNull != std::wstring::npos) { value.erase(firstNull); }

            std::wstring cleaned{ };

            for (auto const character : value)
            {
                if (character < 0x20) { continue; }
                cleaned += character;

                if (cleaned.length() >= MaximumManufacturerNameCharacters) { break; }
            }

            return WindowsMidiServicesInternal::TrimmedWStringCopy(cleaned);
        }

        USHORT ReadFirstLanguageId(_In_ HANDLE hubHandle, _In_ ULONG const portNumber) noexcept
        {
            constexpr size_t bufferSize = sizeof(USB_DESCRIPTOR_REQUEST) + MAXIMUM_USB_STRING_LENGTH;

            std::vector<uint8_t> buffer(bufferSize, 0);

            auto request = reinterpret_cast<USB_DESCRIPTOR_REQUEST*>(buffer.data());

            request->ConnectionIndex = portNumber;
            request->SetupPacket.bmRequest = 0x80;
            request->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
            request->SetupPacket.wValue = static_cast<USHORT>(USB_STRING_DESCRIPTOR_TYPE << 8);   // index 0
            request->SetupPacket.wIndex = 0;
            request->SetupPacket.wLength = MAXIMUM_USB_STRING_LENGTH;

            DWORD bytesReturned{ 0 };

            if (!DeviceIoControlWithTimeout(
                hubHandle,
                IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
                buffer.data(),
                (DWORD)buffer.size(),
                buffer.data(),
                (DWORD)buffer.size(),
                bytesReturned))
            {
                return FallbackLanguageId;
            }

            if (bytesReturned <= sizeof(USB_DESCRIPTOR_REQUEST)) { return FallbackLanguageId; }

            auto const available = bytesReturned - sizeof(USB_DESCRIPTOR_REQUEST);

            if (available < sizeof(USB_COMMON_DESCRIPTOR) + sizeof(USHORT)) { return FallbackLanguageId; }

            auto descriptor = reinterpret_cast<USB_STRING_DESCRIPTOR*>(request->Data);

            if (descriptor->bDescriptorType != USB_STRING_DESCRIPTOR_TYPE) { return FallbackLanguageId; }
            if (descriptor->bLength < sizeof(USB_COMMON_DESCRIPTOR) + sizeof(USHORT)) { return FallbackLanguageId; }
            if (descriptor->bLength > available) { return FallbackLanguageId; }

            auto const languageId = descriptor->bString[0];

            return languageId != 0 ? languageId : FallbackLanguageId;
        }
    }

    _Use_decl_annotations_
    UsbDeviceStrings GetUsbDeviceStrings(std::wstring const& usbDeviceInstanceId) noexcept
    {
        UsbDeviceStrings strings{ };

        try
        {
            if (usbDeviceInstanceId.empty()) { return strings; }

            // composite children and vendor buses do not answer hub requests for themselves
            if (!WindowsMidiServicesInternal::IsStandardUsbDeviceInstanceId(usbDeviceInstanceId)) { return strings; }

            std::wstring hubInterfacePath{ };
            ULONG portNumber{ 0 };

            if (!TryGetHubPathAndPort(usbDeviceInstanceId, hubInterfacePath, portNumber)) { return strings; }

            wil::unique_hfile hub(CreateFileW(
                hubInterfacePath.c_str(),
                GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED,
                nullptr));

            if (!hub) { return strings; }

            // the device descriptor says which string index carries each of these
            std::vector<uint8_t> connectionInfo(sizeof(USB_NODE_CONNECTION_INFORMATION_EX), 0);

            auto info = reinterpret_cast<USB_NODE_CONNECTION_INFORMATION_EX*>(connectionInfo.data());
            info->ConnectionIndex = portNumber;

            DWORD bytesReturned{ 0 };

            if (!DeviceIoControlWithTimeout(
                hub.get(),
                IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
                connectionInfo.data(),
                (DWORD)connectionInfo.size(),
                connectionInfo.data(),
                (DWORD)connectionInfo.size(),
                bytesReturned))
            {
                return strings;
            }

            if (bytesReturned < sizeof(USB_NODE_CONNECTION_INFORMATION_EX)) { return strings; }

            auto const manufacturerIndex = info->DeviceDescriptor.iManufacturer;
            auto const serialNumberIndex = info->DeviceDescriptor.iSerialNumber;

            if (manufacturerIndex == 0 && serialNumberIndex == 0) { return strings; }

            auto const languageId = ReadFirstLanguageId(hub.get(), portNumber);

            strings.Manufacturer = ReadStringDescriptor(hub.get(), portNumber, manufacturerIndex, languageId);
            strings.SerialNumber = ReadStringDescriptor(hub.get(), portNumber, serialNumberIndex, languageId);
        }
        CATCH_LOG();

        return strings;
    }
}
