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

#ifndef MIDI2CPP_MIDICIPROCESSOR_H
#define MIDI2CPP_MIDICIPROCESSOR_H

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <array> // this was missing and causing build errors
#include <functional>
#include <string>

#include "utils.h"

typedef std::tuple<uint32_t, uint8_t> reqId;  //muid-requestId

struct MIDICI{
    MIDICI() : umpGroup(255), deviceId(FUNCTION_BLOCK),ciType(255),ciVer(1), remoteMUID(0), localMUID(0),
        _reqTupleSet(false), totalChunks(0), numChunk(0), partialChunkCount(0), requestId(255) {}
    uint8_t umpGroup;
    uint8_t deviceId;
    uint8_t ciType;
    uint8_t ciVer;
    uint32_t remoteMUID;
    uint32_t localMUID;
    bool _reqTupleSet;
    reqId _peReqIdx;

    uint8_t totalChunks;
    uint8_t numChunk;
    uint8_t partialChunkCount;
    uint8_t requestId;
};




class midiCIProcessor{
private:
    MIDICI midici;
    uint8_t buffer[256];
    /*
     * in Discovery this is [sysexID1,sysexID2,sysexID3,famId1,famid2,modelId1,modelId2,ver1,ver2,ver3,ver4,...product Id]
     * in Profiles this is [pf1, pf1, pf3, pf4, pf5]
     * in Protocols this is [pr1, pr2, pr3, pr4, pr5]
     */

    uint16_t intTemp[4];
    /* in Discovery this is [ciSupport, maxSysex, output path id]
     * in Profile Inquiry Reply, this is [Enabled Profiles Length, Disabled Profile Length]
     * in Profile On/Off/Enabled/Disabled, this is [numOfChannels]
     * in PE this is [header length, Body Length]
     */
    uint16_t sysexPos;

    //MIDI-CI  callbacks

    // EB: update callbacks step1 - update pointer definitions to:
    // std::function<void(..params..)> name = nullptr;

    std::function<bool(uint8_t group, uint32_t muid)>
                        checkMUID = nullptr;
    std::function<void(MIDICI ciDetails,
                                 std::array<uint8_t, 3> manuId, std::array<uint8_t, 2> familyId,
                                 std::array<uint8_t, 2> modelId, std::array<uint8_t, 4> version, uint8_t ciSupport,
                                 uint16_t maxSysex, uint8_t outputPathId)> recvDiscoveryRequest = nullptr;
    std::function<void(MIDICI ciDetails, std::array<uint8_t, 3> manuId, std::array<uint8_t, 2> familyId,
                               std::array<uint8_t, 2> modelId, std::array<uint8_t, 4> version, uint8_t ciSupport, uint16_t maxSysex,
                               uint8_t outputPathId,
                               uint8_t fbIdx)> recvDiscoveryReply = nullptr;
    std::function<void(MIDICI ciDetails, uint8_t status)> recvEndPointInfo = nullptr;
    std::function<void(MIDICI ciDetails, uint8_t status, uint16_t infoLength,
                                  uint8_t* infoData)> recvEndPointInfoReply = nullptr;
    std::function<void(MIDICI ciDetails, uint8_t origSubID, uint8_t statusCode,
                    uint8_t statusData, uint8_t* ackNakDetails, uint16_t messageLength,
                    uint8_t* ackNakMessage)> recvNAK = nullptr;
    std::function<void(MIDICI ciDetails, uint8_t origSubID, uint8_t statusCode,
                    uint8_t statusData, uint8_t* ackNakDetails, uint16_t messageLength,
                    uint8_t* ackNakMessage)> recvACK = nullptr;
    std::function<void(MIDICI ciDetails, uint32_t terminateMuid)> recvInvalidateMUID = nullptr;
    std::function<void(MIDICI ciDetails, uint8_t s7Byte)> recvUnknownMIDICI = nullptr;

//Protocol Negotiation
    std::function<void(MIDICI ciDetails, uint8_t authorityLevel, uint8_t* protocol)> recvProtocolAvailable = nullptr;
    std::function<void(MIDICI ciDetails, uint8_t authorityLevel, uint8_t* protocol)> recvSetProtocol = nullptr;
    std::function<void(MIDICI ciDetails, uint8_t authorityLevel)> recvSetProtocolConfirm = nullptr;
    std::function<void(MIDICI ciDetails, uint8_t authorityLevel, bool testDataAccurate)> recvProtocolTest = nullptr;

    void processProtocolSysex(uint8_t s7Byte);

//Profiles
    std::function<void(MIDICI ciDetails)> recvProfileInquiry = nullptr;
    std::function<void(MIDICI ciDetails, std::array<uint8_t, 5> profile, uint8_t numberOfChannels)> recvSetProfileEnabled = nullptr;
    std::function<void(MIDICI ciDetails, std::array<uint8_t, 5>)> recvSetProfileRemoved = nullptr;
    std::function<void(MIDICI ciDetails, std::array<uint8_t, 5>, uint8_t numberOfChannels)> recvSetProfileDisabled = nullptr;
    std::function<void(MIDICI ciDetails, std::array<uint8_t, 5> profile, uint8_t numberOfChannels)> recvSetProfileOn = nullptr;
    std::function<void(MIDICI ciDetails, std::array<uint8_t, 5> profile)> recvSetProfileOff = nullptr;
    std::function<void(MIDICI ciDetails, std::array<uint8_t, 5> profile, uint16_t datalen, uint8_t*  data,
                               uint16_t part, bool lastByteOfSet)> recvProfileSpecificData = nullptr;
    std::function<void(MIDICI ciDetails, std::array<uint8_t, 5> profile, uint8_t InquiryTarget)> recvSetProfileDetailsInquiry = nullptr;
    std::function<void(MIDICI ciDetails, std::array<uint8_t, 5> profile, uint8_t InquiryTarget,
                                       uint16_t datalen, uint8_t*  data)> recvSetProfileDetailsReply = nullptr;

    void processProfileSysex(uint8_t s7Byte);

//Property Exchange
    std::map<reqId ,std::string> peHeaderStr;

    std::function<void(MIDICI ciDetails, uint8_t numSimulRequests, uint8_t majVer, uint8_t minVer)>  recvPECapabilities = nullptr;
    std::function<void(MIDICI ciDetails, uint8_t numSimulRequests, uint8_t majVer, uint8_t minVer)> recvPECapabilitiesReplies = nullptr;
    std::function<void(MIDICI ciDetails, std::string requestDetails)> recvPEGetInquiry = nullptr;
    std::function<void(MIDICI ciDetails, std::string requestDetails)> recvPESetReply = nullptr;
    std::function<void(MIDICI ciDetails, std::string requestDetails)> recvPESubReply = nullptr;
    std::function<void(MIDICI ciDetails, std::string requestDetails)> recvPENotify = nullptr;
    std::function<void(MIDICI ciDetails, std::string requestDetails, uint16_t bodyLen, uint8_t*  body,
                             bool lastByteOfChunk, bool lastByteOfSet)> recvPEGetReply = nullptr;
    std::function<void(MIDICI ciDetails, std::string requestDetails, uint16_t bodyLen, uint8_t*  body,
                             bool lastByteOfChunk, bool lastByteOfSet)> recvPESetInquiry = nullptr;
    std::function<void(MIDICI ciDetails, std::string requestDetails, uint16_t bodyLen, uint8_t*  body,
                             bool lastByteOfChunk, bool lastByteOfSet)> recvPESubInquiry = nullptr;

    void cleanupRequest(reqId peReqIdx);

    void processPESysex(uint8_t s7Byte);

//Process Inquiry
    std::function<void(MIDICI ciDetails)> recvPICapabilities = nullptr;
    std::function<void(MIDICI ciDetails, uint8_t supportedFeatures)> recvPICapabilitiesReply = nullptr;

    std::function<void(MIDICI ciDetails, uint8_t MDC, uint8_t systemBitmap,
                           uint8_t chanContBitmap, uint8_t chanNoteBitmap)> recvPIMMReport = nullptr;
    std::function<void(MIDICI ciDetails, uint8_t systemBitmap,
                                uint8_t chanContBitmap, uint8_t chanNoteBitmap)> recvPIMMReportReply = nullptr;
    std::function<void(MIDICI ciDetails)> recvPIMMReportEnd = nullptr;

    void processPISysex(uint8_t s7Byte);

public:

    // EB: update callbacks step2 - update setCallback functions:
    // inline void setCallback(std::function<void(params)> fptr){ pointerName = fptr; }
    //
    // Calling these functions from within a member class looks like:
    // MIDICIHandler->setCheckMUID(std::bind(&YourClass::checkMUID, this, std::placeholders::_1, std::placeholders::_2));

    inline void setCheckMUID(std::function<bool(uint8_t group, uint32_t muid)> fptr){
        checkMUID = fptr; }
    void endSysex7();
    void startSysex7(uint8_t group, uint8_t deviceId);
    void processMIDICI(uint8_t s7Byte);


    inline void setRecvDiscovery(std::function<void(MIDICI ciDetails,
                                                    std::array<uint8_t, 3> manuId,
                                                    std::array<uint8_t, 2> familyId,
                                                    std::array<uint8_t, 2> modelId,
                                                    std::array<uint8_t, 4> version,
                                                    uint8_t ciSupport, uint16_t maxSysex,
                                                    uint8_t outputPathId
                                                    )> fptr) {
        recvDiscoveryRequest = fptr;}
    inline void setRecvDiscoveryReply(std::function<void(MIDICI ciDetails,
                                                    std::array<uint8_t, 3> manuId,
                                                    std::array<uint8_t, 2> familyId,
                                                    std::array<uint8_t, 2> modelId,
                                                    std::array<uint8_t, 4> version,
                                                    uint8_t ciSupport, uint16_t maxSysex,
                                                    uint8_t outputPathId,
                                                    uint8_t fbIdx
                                                    )> fptr) {
        recvDiscoveryReply = fptr;}
    inline void setRecvNAK(std::function<void(MIDICI ciDetails, uint8_t origSubID, uint8_t statusCode,
                                                    uint8_t statusData, uint8_t* ackNakDetails, uint16_t messageLength,
                                                    uint8_t* ackNakMessage
                                                    )> fptr){
        recvNAK = fptr;}
    inline void setRecvACK(std::function<void(MIDICI ciDetails, uint8_t origSubID, uint8_t statusCode,
                                                    uint8_t statusData, uint8_t* ackNakDetails, uint16_t messageLength,
                                                    uint8_t* ackNakMessage
                                                    )> fptr){
        recvACK = fptr;}
    inline void setRecvInvalidateMUID(std::function<void(MIDICI ciDetails, uint32_t terminateMuid)> fptr){
        recvInvalidateMUID = fptr;}
    inline void setRecvUnknownMIDICI(std::function<void(MIDICI ciDetails, uint8_t s7Bye)> fptr){
        recvUnknownMIDICI = fptr;}


    inline void setRecvEndpointInfo(std::function<void(MIDICI ciDetails, uint8_t status)> fptr){
        recvEndPointInfo = fptr;}
    inline void setRecvEndpointInfoReply(std::function<void(MIDICI ciDetails, uint8_t status, uint16_t infoLength, uint8_t* infoData)> fptr){
        recvEndPointInfoReply = fptr;}

    //Protocol Negotiation
    inline void setRecvProtocolAvailable(std::function<void(MIDICI ciDetails, uint8_t authorityLevel, uint8_t* protocol)> fptr){
        recvProtocolAvailable = fptr;}
    inline void setRecvSetProtocol(std::function<void(MIDICI ciDetails, uint8_t authorityLevel,uint8_t* protocol)> fptr){
        recvSetProtocol = fptr;}
    inline void setRecvSetProtocolConfirm(std::function<void(MIDICI ciDetails, uint8_t authorityLevel)> fptr){
        recvSetProtocolConfirm = fptr;}
    inline void setRecvSetProtocolTest(std::function<void(MIDICI ciDetails, uint8_t authorityLevel, bool testDataAccurate)> fptr){
        recvProtocolTest = fptr;}

    //Profiles
    inline void setRecvProfileInquiry(std::function<void(MIDICI ciDetails)> fptr){ recvProfileInquiry = fptr;}
    inline void setRecvProfileEnabled(std::function<void(MIDICI ciDetails, std::array<uint8_t, 5>, uint8_t numberOfChannels)> fptr){
        recvSetProfileEnabled = fptr;}
    inline void setRecvSetProfileRemoved(std::function<void(MIDICI ciDetails, std::array<uint8_t, 5>)> fptr){
        recvSetProfileRemoved = fptr;}
    inline void setRecvProfileDisabled(std::function<void(MIDICI ciDetails, std::array<uint8_t, 5>, uint8_t numberOfChannels)> fptr){
        recvSetProfileDisabled = fptr;}
    inline void setRecvProfileOn(std::function<void(MIDICI ciDetails, std::array<uint8_t, 5> profile, uint8_t numberOfChannels)> fptr){
        recvSetProfileOn = fptr;}
    inline void setRecvProfileOff(std::function<void(MIDICI ciDetails, std::array<uint8_t, 5> profile)> fptr){
        recvSetProfileOff = fptr;}
    inline void setRecvProfileSpecificData(std::function<void(MIDICI ciDetails, std::array<uint8_t, 5> profile, uint16_t datalen,
                                                    uint8_t*  data, uint16_t part, bool lastByteOfSet)> fptr){
        recvProfileSpecificData = fptr;}
    inline void setRecvProfileDetailsInquiry(std::function<void(MIDICI ciDetails, std::array<uint8_t, 5> profile,
                                                    uint8_t InquiryTarget)> fptr){
        recvSetProfileDetailsInquiry = fptr;}
    inline void setRecvProfileDetailsReply(std::function<void(MIDICI ciDetails, std::array<uint8_t, 5> profile,
                                                    uint8_t InquiryTarget, uint16_t datalen, uint8_t*  data)> fptr){
        recvSetProfileDetailsReply = fptr;}

    //Property Exchange
    inline void setPECapabilities(std::function<void(MIDICI ciDetails, uint8_t numSimulRequests, uint8_t majVer, uint8_t minVer)> fptr){
        recvPECapabilities = fptr;}
    inline void setPECapabilitiesReply(std::function<void(MIDICI ciDetails, uint8_t numSimulRequests, uint8_t majVer, uint8_t minVer)> fptr){
        recvPECapabilitiesReplies = fptr;}
    inline void setRecvPEGetInquiry(std::function<void(MIDICI ciDetails,  std::string requestDetails)> fptr){
        recvPEGetInquiry = fptr;}
    inline void setRecvPESetReply(std::function<void(MIDICI ciDetails,  std::string requestDetails)> fptr){
        recvPESetReply = fptr;}
    inline void setRecvPESubReply(std::function<void(MIDICI ciDetails,  std::string requestDetails)> fptr){
        recvPESubReply = fptr;}
    inline void setRecvPENotify(std::function<void(MIDICI ciDetails,  std::string requestDetails)> fptr){
        recvPENotify = fptr;}
    inline void setRecvPEGetReply(std::function<void(MIDICI ciDetails,  std::string requestDetails, uint16_t bodyLen, uint8_t* body,
                                                       bool lastByteOfChunk, bool lastByteOfSet)> fptr){
        recvPEGetReply = fptr;}
    inline void setRecvPESetInquiry(std::function<void(MIDICI ciDetails,  std::string requestDetails, uint16_t bodyLen, uint8_t* body,
                                                    bool lastByteOfChunk, bool lastByteOfSet)> fptr){
        recvPESetInquiry = fptr;}
    inline void setRecvPESubInquiry(std::function<void(MIDICI ciDetails,  std::string requestDetails, uint16_t bodyLen, uint8_t* body,
                                                    bool lastByteOfChunk, bool lastByteOfSet)> fptr){
        recvPESubInquiry = fptr;}

//Process Inquiry

    inline void setRecvPICapabilities(std::function<void(MIDICI ciDetails)> fptr){
        recvPICapabilities = fptr;}
    inline void setRecvPICapabilitiesReply(std::function<void(MIDICI ciDetails, uint8_t supportedFeatures)> fptr){
        recvPICapabilitiesReply = fptr;}
    inline void setRecvPIMMReport(std::function<void(MIDICI ciDetails, uint8_t MDC, uint8_t systemBitmap, uint8_t chanContBitmap, uint8_t chanNoteBitmap)> fptr){
        recvPIMMReport = fptr;}
    inline void setRecvPIMMReportReply(std::function<void(MIDICI ciDetails, uint8_t systemBitmap, uint8_t chanContBitmap, uint8_t chanNoteBitmap)> fptr){
        recvPIMMReportReply = fptr;}
    inline void setRecvPIMMEnd(std::function<void(MIDICI ciDetails)> fptr){
        recvPIMMReportEnd = fptr;}

};

#endif //MIDI2CPP_MIDICIPROCESSOR_H
