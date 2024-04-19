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

#include "midiCIMessageCreate.h"

void setBytesFromNumbers(uint8_t *message, uint32_t number, uint16_t *start, uint8_t amount) {
    for (int amountC = amount; amountC > 0; amountC--) {
        message[(*start)++] = number & 127;
        number = number >> 7;
    }
}

void concatSysexArray(uint8_t *sysex, uint16_t *start, uint8_t *add, uint16_t len) {
    uint16_t i;
    for (i = 0; i < len; i++) {
        sysex[(*start)++] = add[i];
    }
}

void createCIHeader(uint8_t *sysexHeader, uint8_t deviceId, uint8_t ciType, uint8_t ciVer, uint32_t localMUID,
                     uint32_t remoteMUID) {
    sysexHeader[0] = S7UNIVERSAL_NRT;
    sysexHeader[1] = deviceId;
    sysexHeader[2] = S7MIDICI;
    sysexHeader[3] = ciType;
    sysexHeader[4] = ciVer;
    uint16_t length = 5;
    setBytesFromNumbers(sysexHeader, localMUID, &length, 4);
    setBytesFromNumbers(sysexHeader, remoteMUID, &length, 4);
}

uint16_t sendDiscovery(uint8_t *sysex, uint8_t midiCIVer, uint8_t ciType, uint32_t srcMUID, uint32_t destMUID,
                       std::array<uint8_t, 3> manuId, std::array<uint8_t, 2> familyId,
                       std::array<uint8_t, 2> modelId, std::array<uint8_t, 4> version,
                       uint8_t ciSupport, uint32_t sysExMax,
                       uint8_t outputPathId,
                       uint8_t fbIdx
) {

    createCIHeader(sysex, 0x7F, ciType, midiCIVer, srcMUID, destMUID);
    uint16_t length = 13;
    concatSysexArray(sysex, &length, manuId.data(), 3);
    concatSysexArray(sysex, &length, familyId.data(), 2);
    concatSysexArray(sysex, &length, modelId.data(), 2);
    concatSysexArray(sysex, &length, version.data(), 4);

    //Capabilities
    sysex[length++] = ciSupport;
    setBytesFromNumbers(sysex, sysExMax, &length, 4);
    if (midiCIVer < 2) {
        return length;
    }
    sysex[length++] = outputPathId;

    if (ciType == MIDICI_DISCOVERY) {
        return length;
    } else {
        sysex[length++] = fbIdx;
        return length;
    }
}

uint16_t CIMessage::sendDiscoveryRequest(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID,
                                         std::array<uint8_t, 3> manuId, std::array<uint8_t, 2> familyId,
                                         std::array<uint8_t, 2> modelId, std::array<uint8_t, 4> version,
                                         uint8_t ciSupport, uint32_t sysExMax,
                                         uint8_t outputPathId
) {
    return sendDiscovery(sysex, midiCIVer, MIDICI_DISCOVERY, srcMUID, M2_CI_BROADCAST,
                         manuId, familyId,
                         modelId, version,
                         ciSupport, sysExMax,
                         outputPathId,
                         0
    );
}

uint16_t CIMessage::sendDiscoveryReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                                       std::array<uint8_t, 3> manuId, std::array<uint8_t, 2> familyId,
                                       std::array<uint8_t, 2> modelId, std::array<uint8_t, 4> version,
                                       uint8_t ciSupport, uint32_t sysExMax,
                                       uint8_t outputPathId,
                                       uint8_t fbIdx
) {
    return sendDiscovery(sysex, midiCIVer, MIDICI_DISCOVERYREPLY, srcMUID, destMUID,
                         manuId, familyId,
                         modelId, version,
                         ciSupport, sysExMax,
                         outputPathId,
                         fbIdx
    );
}

uint16_t
CIMessage::sendEndpointInfoRequest(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                                   uint8_t status) {

    if (midiCIVer < 2) return 0;
    createCIHeader(sysex, 0x7F, MIDICI_ENDPOINTINFO, midiCIVer, srcMUID, destMUID);
    sysex[13] = status;
    return 14;
}

uint16_t
CIMessage::sendEndpointInfoReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID, uint8_t status,
                                 uint16_t infoLength, uint8_t *infoData) {
    if (midiCIVer < 2) return 0;
    createCIHeader(sysex, 0x7F, MIDICI_ENDPOINTINFO_REPLY, midiCIVer, srcMUID, destMUID);
    sysex[13] = status;
    uint16_t length = 14;
    setBytesFromNumbers(sysex, infoLength, &length, 2);
    concatSysexArray(sysex, &length, infoData, infoLength);
    return length;
}


uint16_t sendACKNAK(uint8_t *sysex, uint8_t midiCIVer, uint8_t ciType, uint32_t srcMUID, uint32_t destMUID,
                     uint8_t destination, uint8_t originalSubId, uint8_t statusCode,
                     uint8_t statusData, uint8_t *ackNakDetails, uint16_t messageLength,
                     uint8_t *ackNakMessage) {
    createCIHeader(sysex, destination, ciType, midiCIVer, srcMUID, destMUID);

    uint16_t length = 13;
    if (midiCIVer < 2) {
        return length;
    }

    sysex[length++] = originalSubId;
    sysex[length++] = statusCode;
    sysex[length++] = statusData;

    concatSysexArray(sysex, &length, ackNakDetails, 5);
    setBytesFromNumbers(sysex, messageLength, &length, 2);
    concatSysexArray(sysex, &length, ackNakMessage, messageLength);
    return length;
}

uint16_t CIMessage::sendACK(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID, uint8_t destination,
                            uint8_t originalSubId, uint8_t statusCode,
                            uint8_t statusData, uint8_t *ackNakDetails, uint16_t messageLength,
                            uint8_t *ackNakMessage) {

    return sendACKNAK(sysex, midiCIVer, MIDICI_ACK, srcMUID, destMUID, destination, originalSubId, statusCode,
                       statusData, ackNakDetails,
                       messageLength, ackNakMessage);

}

uint16_t CIMessage::sendNAK(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID, uint8_t destination,
                            uint8_t originalSubId, uint8_t statusCode,
                            uint8_t statusData, uint8_t *ackNakDetails, uint16_t messageLength,
                            uint8_t *ackNakMessage) {

    return sendACKNAK(sysex, midiCIVer, MIDICI_NAK, srcMUID, destMUID, destination, originalSubId, statusCode,
                       statusData, ackNakDetails,
                       messageLength, ackNakMessage);

}

uint16_t CIMessage::sendInvalidateMUID(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t terminateMuid) {
    createCIHeader(sysex, 0x7F, MIDICI_INVALIDATEMUID, midiCIVer, srcMUID, M2_CI_BROADCAST);
    setBytesFromNumbers(sysex, terminateMuid, 0, 4);
    return 17;
}


uint16_t CIMessage::sendProtocolNegotiation(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                                            uint8_t authorityLevel, uint8_t numProtocols, uint8_t *protocols,
                                            uint8_t *currentProtocol) {
    createCIHeader(sysex, 0x7F, MIDICI_PROTOCOL_NEGOTIATION, midiCIVer, srcMUID, destMUID);
    sysex[13] = authorityLevel;
    uint16_t length = 14;
    concatSysexArray(sysex, &length, protocols, numProtocols * 5);
    if (midiCIVer < 2) {
        return length;
    }
    concatSysexArray(sysex, &length, currentProtocol, 5);
    return length;

}

uint16_t CIMessage::sendProtocolNegotiationReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                                                 uint8_t authorityLevel, uint8_t numProtocols, uint8_t *protocols) {
    createCIHeader(sysex, 0x7F, MIDICI_PROTOCOL_NEGOTIATION_REPLY, midiCIVer, srcMUID, destMUID);
    sysex[13] = authorityLevel;
    uint16_t length = 14;
    concatSysexArray(sysex, &length, protocols, numProtocols * 5);
    return length;
}


uint16_t CIMessage::sendSetProtocol(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                                    uint8_t authorityLevel, uint8_t *protocol) {
    createCIHeader(sysex, 0x7F, MIDICI_PROTOCOL_SET, midiCIVer, srcMUID, destMUID);
    sysex[13] = authorityLevel;
    uint16_t length = 14;
    concatSysexArray(sysex, &length, protocol, 5);
    return length;
}

uint16_t CIMessage::sendProtocolTest(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                                     uint8_t authorityLevel) {
    createCIHeader(sysex, 0x7F, MIDICI_PROTOCOL_TEST, midiCIVer, srcMUID, destMUID);
    sysex[13] = authorityLevel;
    uint16_t length = 14;
    uint8_t testData[48] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                            25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};
    concatSysexArray(sysex, &length, testData, 48);
    return length;
}

uint16_t CIMessage::sendProtocolTestResponder(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                                              uint8_t authorityLevel) {
    createCIHeader(sysex, 0x7F, MIDICI_PROTOCOL_TEST_RESPONDER, midiCIVer, srcMUID, destMUID);
    sysex[13] = authorityLevel;
    uint16_t length = 14;
    uint8_t testData[48] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                            25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};
    concatSysexArray(sysex, &length, testData, 48);
    return length;
}

//Profiles

uint16_t CIMessage::sendProfileListRequest(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                                           uint8_t destination) {
    createCIHeader(sysex, destination, MIDICI_PROFILE_INQUIRY, midiCIVer, srcMUID, destMUID);
    return 13;
}


uint16_t
CIMessage::sendProfileListResponse(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                                   uint8_t destination,
                                   uint8_t profilesEnabledLen, uint8_t *profilesEnabled, uint8_t profilesDisabledLen,
                                   uint8_t *profilesDisabled) {
    createCIHeader(sysex, destination, MIDICI_PROFILE_INQUIRYREPLY, midiCIVer, srcMUID, destMUID);
    uint16_t length = 13;
    setBytesFromNumbers(sysex, profilesEnabledLen, &length, 2);
    concatSysexArray(sysex, &length, profilesEnabled, profilesEnabledLen * 5);
    setBytesFromNumbers(sysex, profilesDisabledLen, &length, 2);
    concatSysexArray(sysex, &length, profilesDisabled, profilesDisabledLen * 5);
    return length;
}

uint16_t
sendProfileMessage(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID, uint8_t destination,
                   std::array<uint8_t, 5> profile,
                   uint8_t numberOfChannels, uint8_t ciType) {
    createCIHeader(sysex, destination, ciType, midiCIVer, srcMUID, destMUID);
    uint16_t length = 13;
    concatSysexArray(sysex, &length, profile.data(), 5);
    if (midiCIVer == 1 || ciType == MIDICI_PROFILE_ADD || ciType == MIDICI_PROFILE_REMOVE) {
        return length;
    }
    setBytesFromNumbers(sysex, numberOfChannels, &length, 2);
    return length;

}

uint16_t
CIMessage::sendProfileAdd(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID, uint8_t destination,
                          std::array<uint8_t, 5> profile) {
    return sendProfileMessage(sysex, midiCIVer, srcMUID, destMUID, destination, profile, 0,
                              (uint8_t) MIDICI_PROFILE_ADD);
}

uint16_t
CIMessage::sendProfileRemove(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                             uint8_t destination,
                             std::array<uint8_t, 5> profile) {
    return sendProfileMessage(sysex, midiCIVer, srcMUID, destMUID, destination, profile, 0,
                              (uint8_t) MIDICI_PROFILE_REMOVE);
}

uint16_t
CIMessage::sendProfileOn(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID, uint8_t destination,
                         std::array<uint8_t, 5> profile, uint8_t numberOfChannels) {
    return sendProfileMessage(sysex, midiCIVer, srcMUID, destMUID, destination, profile, numberOfChannels,
                              (uint8_t) MIDICI_PROFILE_SETON);
}

uint16_t
CIMessage::sendProfileOff(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID, uint8_t destination,
                          std::array<uint8_t, 5> profile) {
    return sendProfileMessage(sysex, midiCIVer, srcMUID, destMUID, destination, profile, 0,
                              (uint8_t) MIDICI_PROFILE_SETOFF);
}

uint16_t
CIMessage::sendProfileEnabled(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                              uint8_t destination,
                              std::array<uint8_t, 5> profile,
                              uint8_t numberOfChannels) {
    return sendProfileMessage(sysex, midiCIVer, srcMUID, destMUID, destination, profile, numberOfChannels,
                              (uint8_t) MIDICI_PROFILE_ENABLED);
}

uint16_t
CIMessage::sendProfileDisabled(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                               uint8_t destination,
                               std::array<uint8_t, 5> profile,
                               uint8_t numberOfChannels) {
    return sendProfileMessage(sysex, midiCIVer, srcMUID, destMUID, destination, profile, numberOfChannels,
                              (uint8_t) MIDICI_PROFILE_DISABLED);
}


uint16_t
CIMessage::sendProfileSpecificData(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                                   uint8_t destination,
                                   std::array<uint8_t, 5> profile, uint16_t datalen, uint8_t *data) {
    createCIHeader(sysex, destination, MIDICI_PROFILE_SPECIFIC_DATA, midiCIVer, srcMUID, destMUID);
    uint16_t length = 13;
    concatSysexArray(sysex, &length, profile.data(), 5);
    setBytesFromNumbers(sysex, datalen, &length, 4);
    concatSysexArray(sysex, &length, data, datalen);
    return length;
}

uint16_t CIMessage::sendProfileDetailsInquiry(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                                              uint8_t destination,
                                              std::array<uint8_t, 5> profile, uint8_t InquiryTarget) {
    if (midiCIVer < 2) return 0;
    createCIHeader(sysex, destination, MIDICI_PROFILE_DETAILS_INQUIRY, midiCIVer, srcMUID, destMUID);
    uint16_t length = 13;
    concatSysexArray(sysex, &length, profile.data(), 5);
    sysex[length++] = InquiryTarget;
    return length;
}

uint16_t
CIMessage::sendProfileDetailsReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                                   uint8_t destination,
                                   std::array<uint8_t, 5> profile, uint8_t InquiryTarget, uint16_t datalen,
                                   uint8_t *data) {
    if (midiCIVer < 2) return 0;
    createCIHeader(sysex, destination, MIDICI_PROFILE_DETAILS_REPLY, midiCIVer, srcMUID, destMUID);
    uint16_t length = 13;
    concatSysexArray(sysex, &length, profile.data(), 5);
    sysex[length++] = InquiryTarget;
    setBytesFromNumbers(sysex, datalen, &length, 2);
    concatSysexArray(sysex, &length, data, datalen);
    return length;
}


// Property Exchange

uint16_t CIMessage::sendPECapabilityRequest(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                                            uint8_t numSimulRequests, uint8_t majVer, uint8_t minVer) {
    createCIHeader(sysex, 0x7F, MIDICI_PE_CAPABILITY, midiCIVer, srcMUID, destMUID);
    sysex[13] = numSimulRequests;
    if (midiCIVer == 1) {
        return 14;
    }
    sysex[14] = majVer;
    sysex[15] = minVer;
    return 16;
}

uint16_t CIMessage::sendPECapabilityReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                                          uint8_t numSimulRequests, uint8_t majVer, uint8_t minVer) {
    createCIHeader(sysex, 0x7F, MIDICI_PE_CAPABILITYREPLY, midiCIVer, srcMUID, destMUID);
    sysex[13] = numSimulRequests;
    if (midiCIVer == 1) {
        return 14;
    }
    sysex[14] = majVer;
    sysex[15] = minVer;
    return 16;
}


uint16_t sendPEWithBody(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID, uint8_t requestId,
                        uint16_t headerLen, uint8_t *header, uint16_t numberOfChunks, uint16_t numberOfThisChunk,
                        uint16_t bodyLength, uint8_t *body, uint8_t ciType) {
    createCIHeader(sysex, 0x7F, ciType, midiCIVer, srcMUID, destMUID);
    sysex[13] = requestId;
    uint16_t length = 14;
    setBytesFromNumbers(sysex, headerLen, &length, 2);
    concatSysexArray(sysex, &length, header, headerLen);
    setBytesFromNumbers(sysex, numberOfChunks, &length, 2);
    setBytesFromNumbers(sysex, numberOfThisChunk, &length, 2);
    setBytesFromNumbers(sysex, bodyLength, &length, 2);
    concatSysexArray(sysex, &length, body, bodyLength);
    return length;
}

uint16_t CIMessage::sendPESub(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID, uint8_t requestId,
                              uint16_t headerLen, uint8_t *header, uint16_t numberOfChunks, uint16_t numberOfThisChunk,
                              uint16_t bodyLength, uint8_t *body) {
    return sendPEWithBody(sysex, midiCIVer, srcMUID, destMUID, requestId, headerLen, header, numberOfChunks,
                          numberOfThisChunk,
                          bodyLength, body, (uint8_t) MIDICI_PE_SUB);
}

uint16_t CIMessage::sendPESet(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID, uint8_t requestId,
                              uint16_t headerLen, uint8_t *header, uint16_t numberOfChunks, uint16_t numberOfThisChunk,
                              uint16_t bodyLength, uint8_t *body) {
    return sendPEWithBody(sysex, midiCIVer, srcMUID, destMUID, requestId, headerLen, header, numberOfChunks,
                          numberOfThisChunk,
                          bodyLength, body, (uint8_t) MIDICI_PE_SET);
}

uint16_t
CIMessage::sendPEGetReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID, uint8_t requestId,
                          uint16_t headerLen, uint8_t *header, uint16_t numberOfChunks,
                          uint16_t numberOfThisChunk, uint16_t bodyLength, uint8_t *body) {
    return sendPEWithBody(sysex, midiCIVer, srcMUID, destMUID, requestId, headerLen, header, numberOfChunks,
                          numberOfThisChunk,
                          bodyLength, body, (uint8_t) MIDICI_PE_GETREPLY);
}


uint16_t sendPEHeaderOnly(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID, uint8_t requestId,
                          uint16_t headerLen, uint8_t *header, uint8_t ciType) {
    createCIHeader(sysex, 0x7F, ciType, midiCIVer, srcMUID, destMUID);
    sysex[13] = requestId;
    uint16_t length = 14;
    setBytesFromNumbers(sysex, headerLen, &length, 2);
    concatSysexArray(sysex, &length, header, headerLen);
    setBytesFromNumbers(sysex, 1, &length, 2);
    setBytesFromNumbers(sysex, 1, &length, 2);
    setBytesFromNumbers(sysex, 0, &length, 2);
    return length;
}

uint16_t CIMessage::sendPEGet(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID, uint8_t requestId,
                              uint16_t headerLen, uint8_t *header) {
    return sendPEHeaderOnly(sysex, midiCIVer, srcMUID, destMUID, requestId, headerLen, header,
                            (uint8_t) MIDICI_PE_GET);
}

uint16_t
CIMessage::sendPESubReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID, uint8_t requestId,
                          uint16_t headerLen, uint8_t *header) {
    return sendPEHeaderOnly(sysex, midiCIVer, srcMUID, destMUID, requestId, headerLen, header,
                            (uint8_t) MIDICI_PE_SUBREPLY);
}

uint16_t
CIMessage::sendPENotify(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID, uint8_t requestId,
                        uint16_t headerLen, uint8_t *header) {
    return sendPEHeaderOnly(sysex, midiCIVer, srcMUID, destMUID, requestId, headerLen, header,
                            (uint8_t) MIDICI_PE_NOTIFY);
}

uint16_t
CIMessage::sendPESetReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID, uint8_t requestId,
                          uint16_t headerLen, uint8_t *header) {
    return sendPEHeaderOnly(sysex, midiCIVer, srcMUID, destMUID, requestId, headerLen, header,
                            (uint8_t) MIDICI_PE_SETREPLY);
}

//Process Inquiry
uint16_t CIMessage::sendPICapabilityRequest(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID) {
    if (midiCIVer == 1) return 0;
    createCIHeader(sysex, 0x7F, MIDICI_PI_CAPABILITY, midiCIVer, srcMUID, destMUID);
    return 13;
}

uint16_t CIMessage::sendPICapabilityReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                                          uint8_t supportedFeatures) {
    createCIHeader(sysex, 0x7F, MIDICI_PI_CAPABILITYREPLY, midiCIVer, srcMUID, destMUID);
    sysex[13] = supportedFeatures;
    return 14;
}


uint16_t
CIMessage::sendPIMMReport(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID, uint8_t destination,
                          uint8_t MDC, uint8_t systemBitmap,
                          uint8_t chanContBitmap, uint8_t chanNoteBitmap) {
    createCIHeader(sysex, destination, MIDICI_PI_MM_REPORT, midiCIVer, srcMUID, destMUID);
    sysex[13] = MDC;
    sysex[14] = systemBitmap;
    sysex[15] = 0;
    sysex[16] = chanContBitmap;
    sysex[17] = chanNoteBitmap;
    return 18;
}

uint16_t
CIMessage::sendPIMMReportReply(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                               uint8_t destination,
                               uint8_t systemBitmap,
                               uint8_t chanContBitmap, uint8_t chanNoteBitmap) {
    createCIHeader(sysex, destination, MIDICI_PI_MM_REPORT_REPLY, midiCIVer, srcMUID, destMUID);
    sysex[13] = systemBitmap;
    sysex[14] = 0;
    sysex[15] = chanContBitmap;
    sysex[16] = chanNoteBitmap;
    return 17;
}

uint16_t
CIMessage::sendPIMMReportEnd(uint8_t *sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMUID,
                             uint8_t destination) {
    createCIHeader(sysex, destination, MIDICI_PI_MM_REPORT_END, midiCIVer, srcMUID, destMUID);
    return 13;
}



