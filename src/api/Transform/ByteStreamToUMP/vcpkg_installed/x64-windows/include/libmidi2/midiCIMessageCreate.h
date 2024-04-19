/**********************************************************
 * MIDI 2.0 Library
 * Author: Andrew Mee
 *
 * MIT License
 * Copyright 2022 Andrew Mee
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * ********************************************************/

#ifndef MIDI2CPP_MIDICIMESSAGECREATE_H
#define MIDI2CPP_MIDICIMESSAGECREATE_H
#include "utils.h"
#include <cstdint>
#include <array>

namespace CIMessage {

    uint16_t sendDiscoveryRequest(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID,
                                  std::array<uint8_t, 3> manuId, std::array<uint8_t, 2> familyId,
                                  std::array<uint8_t, 2> modelId, std::array<uint8_t, 4> version,
                                  uint8_t ciSupport, uint32_t sysExMax,
                                  uint8_t outputPathId
    );

    uint16_t sendDiscoveryReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid,
                                std::array<uint8_t, 3> manuId, std::array<uint8_t, 2> familyId,
                                std::array<uint8_t, 2> modelId, std::array<uint8_t, 4> version,
                                uint8_t ciSupport, uint32_t sysExMax,
                                uint8_t outputPathId,
                                uint8_t fbIdx
    );

    uint16_t
    sendEndpointInfoRequest(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t status);

    uint16_t
    sendEndpointInfoReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t status,
                          uint16_t infoLength, uint8_t *infoData);

    uint16_t sendACK(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination,
                     uint8_t originalSubId,        uint8_t statusCode,
                     uint8_t statusData, uint8_t *ackNakDetails, uint16_t messageLength,
                     uint8_t *ackNakMessage);

    uint16_t sendNAK(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination,
                     uint8_t originalSubId,
                     uint8_t statusCode,
                     uint8_t statusData, uint8_t *ackNakDetails, uint16_t messageLength,
                     uint8_t *ackNakMessage);

    uint16_t sendInvalidateMUID(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t terminateMuid);

//Profile Negotiation CI 1.1
    uint16_t sendProtocolNegotiation(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid,
                                     uint8_t authorityLevel, uint8_t numProtocols, uint8_t *protocols,
                                     uint8_t *currentProtocol);

    uint16_t sendProtocolNegotiationReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid,
                                          uint8_t authorityLevel, uint8_t numProtocols, uint8_t *protocols);

    uint16_t sendSetProtocol(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid,
                             uint8_t authorityLevel, uint8_t *protocol);

    uint16_t sendProtocolTest(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid,
                              uint8_t authorityLevel);

    uint16_t sendProtocolTestResponder(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid,
                                       uint8_t authorityLevel);


    uint16_t
    sendProfileListRequest(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination);

    uint16_t
    sendProfileListResponse(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination,
                            uint8_t profilesEnabledLen, uint8_t *profilesEnabled, uint8_t profilesDisabledLen,
                            uint8_t *profilesDisabled);

    uint16_t sendProfileAdd(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination,
                            std::array<uint8_t, 5> profile);

    uint16_t
    sendProfileRemove(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination,
                      std::array<uint8_t, 5> profile);

    uint16_t sendProfileOn(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination,
                           std::array<uint8_t, 5> profile, uint8_t numberOfChannels);

    uint16_t sendProfileOff(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination,
                            std::array<uint8_t, 5> profile);

    uint16_t
    sendProfileEnabled(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination,
                       std::array<uint8_t, 5> profile, uint8_t numberOfChannels);

    uint16_t
    sendProfileDisabled(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination,
                        std::array<uint8_t, 5> profile, uint8_t numberOfChannels);

    uint16_t
    sendProfileSpecificData(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination,
                            std::array<uint8_t, 5> profile, uint16_t datalen, uint8_t *data);

    uint16_t sendProfileDetailsInquiry(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid,
                                       uint8_t destination,
                                       std::array<uint8_t, 5> profile, uint8_t InquiryTarget);

    uint16_t
    sendProfileDetailsReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination,
                            std::array<uint8_t, 5> profile, uint8_t InquiryTarget, uint16_t datalen, uint8_t *data);


    uint16_t sendPECapabilityRequest(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid,
                                     uint8_t numSimulRequests, uint8_t majVer, uint8_t minVer);

    uint16_t sendPECapabilityReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid,
                                   uint8_t numSimulRequests, uint8_t majVer, uint8_t minVer);

    uint16_t sendPEGet(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t requestId,
                       uint16_t headerLen, uint8_t *header);

    uint16_t sendPESet(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t requestId,
                       uint16_t headerLen, uint8_t *header, uint16_t numberOfChunks, uint16_t numberOfThisChunk,
                       uint16_t bodyLength, uint8_t *body);

    uint16_t sendPESub(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t requestId,
                       uint16_t headerLen, uint8_t *header, uint16_t numberOfChunks, uint16_t numberOfThisChunk,
                       uint16_t bodyLength, uint8_t *body);

    uint16_t sendPEGetReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t requestId,
                            uint16_t headerLen, uint8_t *header, uint16_t numberOfChunks, uint16_t numberOfThisChunk,
                            uint16_t bodyLength, uint8_t *body);

    uint16_t sendPESubReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t requestId,
                            uint16_t headerLen, uint8_t *header);

    uint16_t sendPENotify(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t requestId,
                          uint16_t headerLen, uint8_t *header);

    uint16_t sendPESetReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t requestId,
                            uint16_t headerLen, uint8_t *header);


    uint16_t sendPICapabilityRequest(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid);

    uint16_t sendPICapabilityReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid,
                                   uint8_t supportedFeatures);

    uint16_t sendPIMMReport(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination,
                            uint8_t MDC, uint8_t systemBitmap,
                            uint8_t chanContBitmap, uint8_t chanNoteBitmap);

    uint16_t
    sendPIMMReportReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination,
                        uint8_t systemBitmap,
                        uint8_t chanContBitmap, uint8_t chanNoteBitmap);

    uint16_t
    sendPIMMReportEnd(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination);


}
#endif //MIDI2CPP_MIDICIMESSAGECREATE_H
