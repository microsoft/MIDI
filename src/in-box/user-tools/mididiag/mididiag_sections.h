// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// "major.minor.build.revision", or empty when the file has no version resource
std::wstring FileVersionString(_In_ std::wstring const& path);

// What Plug and Play knows about an endpoint's parent device. Empty fields were not available.
struct ParentDeviceDetails
{
    std::wstring LastArrival{};
    std::wstring LastRemoval{};
    std::wstring UsbLocationPath{};
    uint32_t UsbHubCount{ 0 };
    uint32_t ProblemCode{ 0 };
    std::wstring DriverDate{};
};

// mididiag_system.cpp. None of these call the MIDI service.
void CaptureServiceStateBeforeReport();
void OutputOperatingSystemFields();
bool DoSectionApiMode();
bool DoSectionComponentVersions();
bool DoSectionServiceStatus();
bool DoSectionServiceHistory();
bool DoSectionDeviceNodes();
bool DoSectionNetwork();
bool DoSectionMdns();
bool DoSectionNetworkHistory();
bool IsMidiServiceRunning();

// 0 when the service is not installed or cannot be queried
DWORD MidiServiceState();
uint32_t MidiServiceProcessId();

ParentDeviceDetails GetParentDeviceDetails(_In_ std::wstring const& parentInstanceId, _In_ std::wstring const& driverInstanceId);

// mididiag_transports.cpp. These call the MIDI service.
bool DoSectionServiceResponse();
void OutputTransportCapabilities(_In_ winrt::guid const& transportId, _In_ winrt::hstring const& transportCode);
bool DoSectionBluetooth();
bool DoSectionNetworkMidi2();
bool DoSectionRtpMidi();
bool DoSectionLoopback();
bool DoSectionBasicLoopback();
bool DoSectionEndpointCustomizations();
bool DoSectionConnectionTiming();
