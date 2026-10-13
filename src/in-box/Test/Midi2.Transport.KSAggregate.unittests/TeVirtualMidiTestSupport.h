// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// Ports made with the teVirtualMIDI driver, which loopMIDI, rtpMIDI and other software install.
// Each port is a KS filter that appears on, or leaves, the one teVirtualMIDI device, and the KS
// aggregate transport puts all of them in one endpoint. That makes them the simplest way to add
// and remove pins on a device whose endpoint is open.
//
// The driver's functions are declared here from its published interface and loaded at run time,
// so the tests build without its SDK and skip on a PC without the driver, like a build agent. It
// has no Arm64 driver.

namespace TeVirtualMidiTest
{
    using PortHandle = void*;

    using DataCallback = void (CALLBACK*)(PortHandle, LPBYTE, DWORD, DWORD_PTR);
    using CreatePortEx2Function = PortHandle (CALLBACK*)(LPCWSTR, DataCallback, DWORD_PTR, DWORD, DWORD);
    using ClosePortFunction = void (CALLBACK*)(PortHandle);
    using SendDataFunction = BOOL (CALLBACK*)(PortHandle, LPBYTE, DWORD);

    // parse what applications send to the port, and create both its input and its output
    inline constexpr DWORD PortFlags{ 1 | 12 };

    inline constexpr DWORD MaxSysExLength{ 65535 };

    struct Driver
    {
        CreatePortEx2Function CreatePortEx2{ nullptr };
        ClosePortFunction ClosePort{ nullptr };
        SendDataFunction SendData{ nullptr };
    };

    // An x64 or x86 test running under emulation on an Arm64 PC
    inline bool RunningOnArm64() noexcept
    {
        USHORT processMachine{ IMAGE_FILE_MACHINE_UNKNOWN };
        USHORT nativeMachine{ IMAGE_FILE_MACHINE_UNKNOWN };

        return IsWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine) &&
            nativeMachine == IMAGE_FILE_MACHINE_ARM64;
    }

    // The driver's functions, or nullptr and the reason the tests can't use it here
    inline Driver const* LoadDriver(_Out_ std::wstring& unavailableReason)
    {
        unavailableReason.clear();

#if defined(_M_ARM64) || defined(_M_ARM64EC)
        unavailableReason = L"teVirtualMIDI has no Arm64 driver.";
        return nullptr;
#else
        if (RunningOnArm64())
        {
            unavailableReason = L"teVirtualMIDI has no Arm64 driver.";
            return nullptr;
        }

#if defined(_WIN64)
        static constexpr wchar_t libraryName[]{ L"teVirtualMIDI64.dll" };
#else
        static constexpr wchar_t libraryName[]{ L"teVirtualMIDI32.dll" };
#endif

        // never freed: the driver calls back on its own threads for as long as a port is open
        static HMODULE const library = LoadLibraryExW(libraryName, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

        if (library == nullptr)
        {
            unavailableReason = std::wstring{ libraryName } + L" is not installed. It comes with loopMIDI and other teVirtualMIDI based software.";
            return nullptr;
        }

        static Driver const driver = []
            {
                Driver loaded{ };
                loaded.CreatePortEx2 = reinterpret_cast<CreatePortEx2Function>(GetProcAddress(library, "virtualMIDICreatePortEx2"));
                loaded.ClosePort = reinterpret_cast<ClosePortFunction>(GetProcAddress(library, "virtualMIDIClosePort"));
                loaded.SendData = reinterpret_cast<SendDataFunction>(GetProcAddress(library, "virtualMIDISendData"));

                return loaded;
            }();

        if (driver.CreatePortEx2 == nullptr || driver.ClosePort == nullptr || driver.SendData == nullptr)
        {
            unavailableReason = std::wstring{ libraryName } + L" does not have the functions these tests use.";
            return nullptr;
        }

        return &driver;
#endif
    }


    // One port, and what applications have sent to it. It must not move once created, because
    // the driver calls back with its address.
    class Port
    {
    public:
        explicit Port(_In_ Driver const& driver) noexcept :
            m_driver{ driver }
        {
        }

        ~Port()
        {
            Close();
        }

        Port(Port const&) = delete;
        Port& operator=(Port const&) = delete;

        HRESULT Create(_In_ std::wstring const& name) noexcept try
        {
            Close();

            m_name = name;
            m_handle = m_driver.CreatePortEx2(name.c_str(), &Port::OnData, reinterpret_cast<DWORD_PTR>(this), MaxSysExLength, PortFlags);

            RETURN_LAST_ERROR_IF_NULL(m_handle);

            return S_OK;
        }
        CATCH_RETURN();

        // Out of the port, to an application reading it
        bool Send(_In_ std::vector<uint8_t> message) noexcept
        {
            return m_handle != nullptr && m_driver.SendData(m_handle, message.data(), static_cast<DWORD>(message.size()));
        }

        // Removes the port from the device
        void Close() noexcept
        {
            if (m_handle != nullptr)
            {
                m_driver.ClosePort(m_handle);
                m_handle = nullptr;
            }
        }

        // An application sent this message to the port
        bool WaitForReceived(_In_ std::vector<uint8_t> const& message, _In_ DWORD const timeoutMilliseconds)
        {
            return MidiTest::WaitUntil([&]
                {
                    auto lock = m_lock.lock();

                    return std::find(m_received.begin(), m_received.end(), message) != m_received.end();
                },
                timeoutMilliseconds);
        }

        std::wstring const& Name() const noexcept
        {
            return m_name;
        }

    private:
        static void CALLBACK OnData(_In_ PortHandle, _In_reads_bytes_opt_(length) LPBYTE data, _In_ DWORD length, _In_ DWORD_PTR instance) noexcept
        {
            // no data when the port is closing
            if (data == nullptr || length == 0 || instance == 0)
            {
                return;
            }

            auto const port = reinterpret_cast<Port*>(instance);

            try
            {
                auto lock = port->m_lock.lock();
                port->m_received.emplace_back(data, data + length);
            }
            catch (...)
            {
            }
        }

        Driver const& m_driver;
        PortHandle m_handle{ nullptr };
        std::wstring m_name{ };

        wil::critical_section m_lock{ };
        std::vector<std::vector<uint8_t>> m_received{ };
    };


    // Where the service put a port: the endpoint, and the group and MIDI 1.0 port of each direction
    struct PortLocation
    {
        std::wstring EndpointDeviceId{ };
        uint8_t SourceGroupIndex{ 0 };          // carries what the port sends to applications
        uint8_t DestinationGroupIndex{ 0 };     // carries what applications send to the port
        std::wstring SourcePortInterfaceId{ };
        std::wstring DestinationPortInterfaceId{ };
    };

    struct Midi1Port
    {
        std::wstring EndpointDeviceId{ };
        uint8_t GroupIndex{ 0 };
        std::wstring InterfaceId{ };
    };

    // DEVINTERFACE_MIDI_INPUT and DEVINTERFACE_MIDI_OUTPUT: the MIDI 1.0 ports the service publishes
    inline constexpr wchar_t Midi1SourcePortInterfaceClass[]{ L"{504BE32C-CCF6-4D2C-B73F-6F8B3747E22B}" };
    inline constexpr wchar_t Midi1DestinationPortInterfaceClass[]{ L"{6DC23320-AB33-4CE4-80D4-BBB3EBBF2814}" };

    // The service's MIDI 1.0 port with this name in it
    inline std::optional<Midi1Port> FindMidi1Port(
        _In_z_ wchar_t const* interfaceClass,
        _In_ std::wstring const& portName) noexcept
    {
        try
        {
            std::wstring selector{ L"System.Devices.InterfaceClassGuid:=\"" };
            selector += interfaceClass;
            selector += L"\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";

            auto properties = winrt::single_threaded_vector<winrt::hstring>();
            properties.Append(STRING_PKEY_MIDI_AssociatedUMP);
            properties.Append(STRING_PKEY_MIDI_PortAssignedGroupIndex);

            auto const ports = winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
                selector,
                properties,
                winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

            for (auto const& port : ports)
            {
                if (std::wstring{ port.Name() }.find(portName) == std::wstring::npos)
                {
                    continue;
                }

                auto const endpoint = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(STRING_PKEY_MIDI_AssociatedUMP, port, L"");
                auto const group = internal::SafeGetSwdPropertyFromDeviceInformation<uint32_t>(STRING_PKEY_MIDI_PortAssignedGroupIndex, port, UINT32_MAX);

                if (endpoint.empty() || group >= KsaPinMap::MaxGroupCount)
                {
                    continue;
                }

                return Midi1Port{ std::wstring{ endpoint }, static_cast<uint8_t>(group), std::wstring{ port.Id() } };
            }
        }
        catch (...)
        {
        }

        return std::nullopt;
    }

    inline std::optional<PortLocation> FindPort(_In_ std::wstring const& portName) noexcept
    {
        auto const source = FindMidi1Port(Midi1SourcePortInterfaceClass, portName);
        auto const destination = FindMidi1Port(Midi1DestinationPortInterfaceClass, portName);

        if (!source.has_value() || !destination.has_value() ||
            CompareStringOrdinal(source->EndpointDeviceId.c_str(), -1, destination->EndpointDeviceId.c_str(), -1, TRUE) != CSTR_EQUAL)
        {
            return std::nullopt;
        }

        return PortLocation{ source->EndpointDeviceId, source->GroupIndex, destination->GroupIndex, source->InterfaceId, destination->InterfaceId };
    }

    inline bool PortIsGone(_In_ std::wstring const& portName) noexcept
    {
        return !FindMidi1Port(Midi1SourcePortInterfaceClass, portName).has_value() &&
            !FindMidi1Port(Midi1DestinationPortInterfaceClass, portName).has_value();
    }
}
