// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#pragma once

// The transport's COM class id, which is also the id the service knows it by
#define MIDI_RTP_TRANSPORT_ID_FOR_SDK ::winrt::guid{ 0x54c9b2f6, 0xc235, 0x4000, { 0xa6, 0x75, 0x9f, 0x69, 0x58, 0xa1, 0xa4, 0xfa } }

// the DNS-SD service type without the domain, as apps show it
#define MIDI_RTP_DNSSD_SERVICE_TYPE_FOR_SDK L"_apple-midi._udp"
