/**********************************************************
 * MIDI 2.0 Library 
 * Author: Andrew Mee
 * 
 * MIT License
 * Copyright 2021 Andrew Mee
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

#include "midiCIProcessor.h"

void midiCIProcessor::endSysex7(){
    if(midici._reqTupleSet){
        cleanupRequest(midici._peReqIdx);
    }
}

void midiCIProcessor::startSysex7(uint8_t group, uint8_t deviceId){

    sysexPos = 0;
    buffer[0]='\0';
    midici =  MIDICI();
    midici.deviceId = deviceId;
    midici.umpGroup = group;
}

void midiCIProcessor::cleanupRequest(reqId peReqIdx){
    peHeaderStr.erase(peReqIdx);
}

void midiCIProcessor::processMIDICI(uint8_t s7Byte){
    //printf("s7 Byte %d\n", s7Byte);
	if(sysexPos == 3){
		midici.ciType =  s7Byte;
	}
	
	if(sysexPos == 4){
	    midici.ciVer =  s7Byte;
	}
	if(sysexPos >= 5 && sysexPos <= 8){
        midici.remoteMUID += s7Byte << (7 * (sysexPos - 5));
	}
	
	if(sysexPos >= 9 && sysexPos <= 12){
        midici.localMUID += s7Byte << (7 * (sysexPos - 9));
	}
	
	if(sysexPos >= 12
       && midici.localMUID != M2_CI_BROADCAST
       && checkMUID && !checkMUID(midici.umpGroup, midici.localMUID)
        ){
		return; //Not for this device
	}
	
	//break up each Process based on ciType
    if(sysexPos >= 12) {
        switch (midici.ciType) {
            case MIDICI_DISCOVERYREPLY: //Discovery Reply
            case MIDICI_DISCOVERY: { //Discovery Request
                if (sysexPos >= 13 && sysexPos <= 23) {
                    buffer[sysexPos - 13] = s7Byte;
                }
                if (sysexPos == 24) {
                    intTemp[0] = s7Byte; // ciSupport
                }
                if (sysexPos >= 25 && sysexPos <= 28) {
                    intTemp[1] += s7Byte << (7 * (sysexPos - 25)); //maxSysEx
                }

                bool complete = false;
                if (sysexPos == 28 && midici.ciVer == 1) {
                    complete = true;
                }
                else if (sysexPos == 28){
                    intTemp[2] = s7Byte; //output path id
                    if(midici.ciType==MIDICI_DISCOVERY) {
                        complete = true;
                    }
                }
                else if (sysexPos == 29){
                    intTemp[3] = s7Byte; //fbIdx id
                    if(midici.ciType==MIDICI_DISCOVERYREPLY) {
                        complete = true;
                    }
                }

                if (complete) {
                    //debug("  - Discovery Request 28 ");

                    if(midici.ciType==MIDICI_DISCOVERY) {
                        if (recvDiscoveryRequest != nullptr) recvDiscoveryRequest(

                                midici,
                                {buffer[0],buffer[1],buffer[2]},
                                {buffer[3], buffer[4]},
                                {buffer[5], buffer[6]},
                                {buffer[7], buffer[8],
                                 buffer[9], buffer[10]},
                                (uint8_t) intTemp[0],
                                intTemp[1],
                                (uint8_t) intTemp[2]
                                //intTemp[3],
                               // &(buffer[11])
                        );
                    }else{
                        if (recvDiscoveryReply != nullptr) recvDiscoveryReply(

                                midici,
                                {buffer[0],buffer[1],buffer[2]},
                                {buffer[3], buffer[4]},
                                {buffer[5], buffer[6]},
                                {buffer[7], buffer[8],
                                 buffer[9], buffer[10]},
                                (uint8_t) intTemp[0],
                                intTemp[1],
                                (uint8_t) intTemp[2],
                                (uint8_t) intTemp[3]
                                //&(buffer[11])
                        );
                    }
                }
                break;
            }

            case MIDICI_INVALIDATEMUID: //MIDI-CI Invalidate MUID Message

                if (sysexPos >= 13 && sysexPos <= 16) {
                    buffer[sysexPos - 13] = s7Byte;
                }

                //terminate MUID
                if (sysexPos == 16 && recvInvalidateMUID != nullptr) {
                    uint32_t terminateMUID = buffer[0]
                            + ((uint32_t)buffer[1] << 7)
                            + ((uint32_t)buffer[2] << 14)
                            + ((uint32_t)buffer[3] << 21);
                    recvInvalidateMUID(midici, terminateMUID);
                }
                break;
            case MIDICI_ENDPOINTINFO:{
                if (sysexPos == 13 && midici.ciVer > 1 && recvEndPointInfo!= nullptr) {
                    recvEndPointInfo(midici,s7Byte); // uint8_t origSubID,
                }
                break;
            }
            case MIDICI_ENDPOINTINFO_REPLY:{
                bool complete = false;
                if(midici.ciVer < 2) return;
                if (sysexPos == 13 && recvEndPointInfo!= nullptr) {
                    intTemp[0] = s7Byte;
                }
                if(sysexPos == 14 || sysexPos == 15){
                    intTemp[1] += s7Byte << (7 * (sysexPos - 14 ));
                    return;
                }
                if (sysexPos >= 16 && sysexPos <= 15 + intTemp[1]){
                    buffer[sysexPos - 16] = s7Byte; //Info Data
                }if (sysexPos == 16 + intTemp[1]){
                    complete = true;
                }

                if (complete) {
                    recvEndPointInfoReply(midici,
                                     (uint8_t) intTemp[0],
                                     intTemp[1],
                                     buffer
                                     );
                }
                break;
            }
            case MIDICI_ACK:
            case MIDICI_NAK: {
                bool complete = false;

                if (sysexPos == 13 && midici.ciVer == 1) {
                    complete = true;
                } else if (sysexPos == 13 && midici.ciVer > 1) {
                    intTemp[0] = s7Byte; // uint8_t origSubID,
                }

                if (sysexPos == 14) {
                    intTemp[1] = s7Byte; //statusCode
                }

                if (sysexPos == 15) {
                    intTemp[2] = s7Byte; //statusData
                }

                if (sysexPos >= 16 && sysexPos <= 20){
                    buffer[sysexPos - 16] = s7Byte; //ackNakDetails
                }

                if(sysexPos == 21 || sysexPos == 22){
                    intTemp[3] += s7Byte << (7 * (sysexPos - 21 ));
                    return;
                }

                if (sysexPos >= 23 && sysexPos <= 23 + intTemp[3]){
                    buffer[sysexPos - 23] = s7Byte; //product ID
                }
                if (sysexPos == 23 + intTemp[3]){
                    complete = true;
                }

                if (complete) {
                    uint8_t ackNakDetails[5] = {buffer[0], buffer[1],
                                                buffer[2],
                                                buffer[3],
                                                buffer[4]};

                    if (midici.ciType == MIDICI_NAK && recvNAK != nullptr)
                        recvNAK(

                            midici,
                            (uint8_t) intTemp[0],
                            (uint8_t) intTemp[1],
                            (uint8_t) intTemp[2],
                            ackNakDetails,
                            intTemp[3],
                            buffer
                    );

                    if (midici.ciType == MIDICI_ACK && midici.ciVer > 1 && recvACK != nullptr)
                        recvACK(

                            midici,
                            (uint8_t) intTemp[0],
                            (uint8_t) intTemp[1],
                            (uint8_t) intTemp[2],
                            ackNakDetails,
                            intTemp[3],
                            buffer
                        );
                }
                break;
            }

#ifdef M2_ENABLE_PROTOCOL
            case MIDICI_PROTOCOL_NEGOTIATION:
            case MIDICI_PROTOCOL_NEGOTIATION_REPLY:
            case MIDICI_PROTOCOL_SET:
            case MIDICI_PROTOCOL_TEST:
            case MIDICI_PROTOCOL_TEST_RESPONDER:
            case MIDICI_PROTOCOL_CONFIRM:
                processProtocolSysex(s7Byte);
                break;
#endif

#ifndef M2_DISABLE_PROFILE
            case MIDICI_PROFILE_INQUIRY: //Profile Inquiry
            case MIDICI_PROFILE_INQUIRYREPLY: //Reply to Profile Inquiry
            case MIDICI_PROFILE_SETON: //Set Profile On Message
            case MIDICI_PROFILE_SETOFF: //Set Profile Off Message
            case MIDICI_PROFILE_ENABLED: //Set Profile Enabled Message
            case MIDICI_PROFILE_DISABLED: //Set Profile Disabled Message
            case MIDICI_PROFILE_SPECIFIC_DATA: //ProfileSpecific Data
            case MIDICI_PROFILE_DETAILS_INQUIRY:
            case MIDICI_PROFILE_DETAILS_REPLY:
                processProfileSysex(s7Byte);
                break;
#endif


#ifndef M2_DISABLE_PE
            case MIDICI_PE_CAPABILITY: //Inquiry: Property Exchange Capabilities
            case MIDICI_PE_CAPABILITYREPLY: //Reply to Property Exchange Capabilities
            case MIDICI_PE_GET:  // Inquiry: Get Property Data
            case MIDICI_PE_GETREPLY: // Reply To Get Property Data - Needs Work!
            case MIDICI_PE_SET: // Inquiry: Set Property Data
            case MIDICI_PE_SETREPLY: // Reply To Inquiry: Set Property Data
            case MIDICI_PE_SUB: // Inquiry: Subscribe Property Data
            case MIDICI_PE_SUBREPLY: // Reply To Subscribe Property Data
            case MIDICI_PE_NOTIFY: // Notify
                processPESysex(s7Byte);
                break;
#endif

#ifndef M2_DISABLE_PROCESSINQUIRY
            case MIDICI_PI_CAPABILITY:
            case MIDICI_PI_CAPABILITYREPLY:
            case MIDICI_PI_MM_REPORT:
            case MIDICI_PI_MM_REPORT_REPLY:
            case MIDICI_PI_MM_REPORT_END:
                processPISysex(s7Byte);
                break;
#endif
            default:
                if (recvUnknownMIDICI) {
                    recvUnknownMIDICI(midici, s7Byte);
                }
                break;

        }
    }
    sysexPos++;
}

void midiCIProcessor::processProtocolSysex(uint8_t s7Byte){
    switch (midici.ciType){

        case MIDICI_PROTOCOL_NEGOTIATION:
        case MIDICI_PROTOCOL_NEGOTIATION_REPLY: {
            //Authority Level
            if (sysexPos == 13 ) {
                intTemp[0] = s7Byte;
            }
            //Number of Supported Protocols (np)
            if (sysexPos == 14 ) {
                intTemp[1] = s7Byte;
            }

            int protocolOffset = intTemp[1] * 5 + 14;

            if (sysexPos >= 15 && sysexPos < protocolOffset) {
                uint8_t pos = (sysexPos - 14) % 5;
                buffer[pos] = s7Byte;
                if (pos == 4 && recvProtocolAvailable != nullptr) {
                    uint8_t protocol[5] = {buffer[0], buffer[1],
                                           buffer[2], buffer[3],
                                           buffer[4]};
                    recvProtocolAvailable(midici, (uint8_t) intTemp[0], protocol);
                }
            }
            if(midici.ciVer > 1){
                if (sysexPos >= protocolOffset && sysexPos <= protocolOffset+5){
                    buffer[sysexPos-protocolOffset] = s7Byte;
                }
                if (sysexPos == protocolOffset+5){
//                    uint8_t protocol[5] = {buffer[0], buffer[1],
//                                           buffer[2], buffer[3],
//                                           buffer[4]};
                    if (recvSetProtocolConfirm != nullptr)recvSetProtocolConfirm(midici, (uint8_t) intTemp[0]);
                }
            }
            break;
        }

        case MIDICI_PROTOCOL_SET: //Set Profile On Message
            //Authority Level
            if (sysexPos == 13 ) {
                intTemp[0] = s7Byte;
            }
            if(sysexPos >= 14 && sysexPos <= 18){
                buffer[sysexPos-13] = s7Byte;
            }
            if (sysexPos == 18 && recvSetProtocol != nullptr){
                uint8_t protocol[5] = {buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]};
                recvSetProtocol(midici, (uint8_t) intTemp[0], protocol);
            }
            break;

        case MIDICI_PROTOCOL_TEST_RESPONDER:
        case MIDICI_PROTOCOL_TEST:
            //Authority Level
            if (sysexPos == 13 ) {
                intTemp[0] = s7Byte;
                intTemp[1] = 1;
            }
            if(sysexPos >= 14 && sysexPos <= 61){
                if(s7Byte != sysexPos - 14){
                    intTemp[1] = 0;
                }
            }
            if (sysexPos == 61 && recvProtocolTest != nullptr){
                recvProtocolTest(midici, (uint8_t) intTemp[0], !!(intTemp[1]));
            }


            break;

        case MIDICI_PROTOCOL_CONFIRM: //Set Profile Off Message
            //Authority Level
            if (sysexPos == 13 ) {
                intTemp[0] = s7Byte;
                if (recvSetProtocolConfirm != nullptr){
                    recvSetProtocolConfirm(midici, (uint8_t) intTemp[0]);
                }
            }
            break;
    }
}

void midiCIProcessor::processProfileSysex(uint8_t s7Byte){
    switch (midici.ciType){
        case MIDICI_PROFILE_INQUIRY: //Profile Inquiry
            if (sysexPos == 12 && recvProfileInquiry != nullptr){
                recvProfileInquiry(midici);
            }
            break;
        case MIDICI_PROFILE_INQUIRYREPLY: { //Reply to Profile Inquiry
            //Enabled Profiles Length
            if (sysexPos == 13 || sysexPos == 14) {
                intTemp[0] += s7Byte << (7 * (sysexPos - 13));
            }

            //Disabled Profile Length
            int enabledProfileOffset = intTemp[0] * 5 + 13;
            if (
                    sysexPos == enabledProfileOffset
                    || sysexPos == 1 + enabledProfileOffset
                    ) {
                intTemp[1] += s7Byte << (7 * (sysexPos - enabledProfileOffset));
            }

            if (sysexPos >= 15 && sysexPos < enabledProfileOffset) {
                uint8_t pos = (sysexPos - 13) % 5;
                buffer[pos] = s7Byte;
                if (pos == 4 && recvSetProfileEnabled != nullptr) {

                    recvSetProfileEnabled(midici, {buffer[0], buffer[1],
                                                   buffer[2], buffer[3],
                                                   buffer[4]},0);
                }
            }

            if (sysexPos >= 2 + enabledProfileOffset &&
                sysexPos < enabledProfileOffset + intTemp[1] * 5) {
                uint8_t pos = (sysexPos - 13) % 5;
                buffer[pos] = s7Byte;
                if (pos == 4 && recvSetProfileDisabled != nullptr) {
                    recvSetProfileDisabled(midici, {buffer[0], buffer[1],
                                                    buffer[2], buffer[3],
                                                    buffer[4]}
                            ,0);
                }
            }
            break;
        }

        case MIDICI_PROFILE_ADD:
        case MIDICI_PROFILE_REMOVE:
        case MIDICI_PROFILE_ENABLED:
        case MIDICI_PROFILE_DISABLED:
        case MIDICI_PROFILE_SETOFF:
        case MIDICI_PROFILE_SETON: { //Set Profile On Message
            bool complete = false;
            if (sysexPos >= 13 && sysexPos <= 17) {
                buffer[sysexPos - 13] = s7Byte;
            }
            if (sysexPos == 17 &&
                (midici.ciVer == 1 || midici.ciType==MIDICI_PROFILE_ADD || midici.ciType==MIDICI_PROFILE_REMOVE)
                    ){
                complete = true;
            }
            if(midici.ciVer > 1 && (sysexPos == 18 || sysexPos == 19)){
                intTemp[0] += s7Byte << (7 * (sysexPos - 18 ));
            }
            if (sysexPos == 19 && midici.ciVer > 1){
                complete = true;
            }

            if(complete){
                if (midici.ciType == MIDICI_PROFILE_ADD && recvSetProfileDisabled != nullptr)
                    recvSetProfileDisabled(midici, {buffer[0], buffer[1],
                                                    buffer[2], buffer[3],
                                                    buffer[4]}, 0);

                if (midici.ciType == MIDICI_PROFILE_REMOVE && recvSetProfileRemoved != nullptr)
                    recvSetProfileRemoved(midici, {buffer[0], buffer[1],
                                                   buffer[2], buffer[3],
                                                   buffer[4]});

                if (midici.ciType == MIDICI_PROFILE_SETON && recvSetProfileOn != nullptr)
                    recvSetProfileOn(midici, {buffer[0], buffer[1],
                                              buffer[2], buffer[3],
                                              buffer[4]}, (uint8_t)intTemp[0]);

                if (midici.ciType == MIDICI_PROFILE_SETOFF && recvSetProfileOff != nullptr)
                    recvSetProfileOff(midici, {buffer[0], buffer[1],
                                               buffer[2], buffer[3],
                                               buffer[4]});

                if (midici.ciType == MIDICI_PROFILE_ENABLED && recvSetProfileEnabled != nullptr)
                    recvSetProfileEnabled(midici, {buffer[0], buffer[1],
                                                   buffer[2], buffer[3],
                                                   buffer[4]}, (uint8_t)intTemp[0]);

                if (midici.ciType == MIDICI_PROFILE_DISABLED && recvSetProfileDisabled != nullptr)
                    recvSetProfileDisabled(midici, {buffer[0], buffer[1],
                                                    buffer[2], buffer[3],
                                                    buffer[4]}, (uint8_t)intTemp[0]);

            }
            break;
        }

        case MIDICI_PROFILE_DETAILS_INQUIRY:{
            if (sysexPos >= 13 && sysexPos <= 17) {
                buffer[sysexPos - 13] = s7Byte;
            }
            if (sysexPos == 18 && recvSetProfileDetailsInquiry != nullptr){ //Inquiry Target
                recvSetProfileDetailsInquiry(midici, {buffer[0], buffer[1],
                                                      buffer[2], buffer[3],
                                                      buffer[4]}, s7Byte);
            }

            break;
        }

        case MIDICI_PROFILE_DETAILS_REPLY:{
            if (sysexPos >= 13 && sysexPos <= 17) {
                buffer[sysexPos - 13] = s7Byte;
            }
            if (sysexPos == 18){//Inquiry Target
                buffer[5] = s7Byte;
            }

            if(sysexPos == 19 || sysexPos == 20){ //Inquiry Target Data length (dl)
                intTemp[0] += s7Byte << (7 * (sysexPos - 19 ));
            }

            if (sysexPos >= 21 && sysexPos <= 21 + intTemp[0]){
                buffer[sysexPos - 22 + 6] = s7Byte; //product ID
            }

            if (sysexPos == 21 + intTemp[0] && recvSetProfileDetailsInquiry != nullptr){
                recvSetProfileDetailsReply(midici, {buffer[0], buffer[1],
                                                    buffer[2], buffer[3],
                                                    buffer[4]},
                                           buffer[5],
                                           intTemp[0],
                                           &(buffer[6])
                );
            }

            break;
        }

        case MIDICI_PROFILE_SPECIFIC_DATA:
            //Profile
            if(sysexPos >= 13 && sysexPos <= 17){
                buffer[sysexPos-13] = s7Byte;
                return;
            }
            if(sysexPos >= 18 && sysexPos <= 21){ //Length of Following Profile Specific Data
                intTemp[0] += s7Byte << (7 * (sysexPos - 18 ));
                intTemp[1] = 1;
                return;
            }


            //******************

            uint16_t charOffset = (sysexPos - 22) % S7_BUFFERLEN;
            uint16_t dataLength = intTemp[0];
            if(
                    (sysexPos >= 22 && sysexPos <= 21 + dataLength)
                    || 	(dataLength == 0 && sysexPos == 21)
                    ){
                if(dataLength != 0 )buffer[charOffset] = s7Byte;

                bool lastByteOfSet = (sysexPos == 21 + dataLength);

                if(charOffset == S7_BUFFERLEN -1
                   || sysexPos == 21 + dataLength
                   || dataLength == 0
                        ){
                    recvProfileSpecificData(midici, {buffer[0], buffer[1],
                                                buffer[2], buffer[3],
                                                buffer[4]}, charOffset+1, buffer, intTemp[1], lastByteOfSet);
                    intTemp[1]++;
                }
            }


            //***********

            break;
    }
}

void midiCIProcessor::processPESysex(uint8_t s7Byte){

    switch (midici.ciType){
        case MIDICI_PE_CAPABILITY:
        case MIDICI_PE_CAPABILITYREPLY:{
            bool complete = false;

            if(sysexPos == 13){
                buffer[0] = s7Byte;
            }

            if(sysexPos == 13 && midici.ciVer == 1){
                complete = true;
            }

            if(sysexPos == 14){
                buffer[1] = s7Byte;
            }
            if(sysexPos == 15){
                buffer[2] = s7Byte;
                complete = true;
            }

            if(complete){
                if(midici.ciType == MIDICI_PE_CAPABILITY && recvPECapabilities != nullptr)
                    recvPECapabilities(midici,
                                       buffer[0],
                                       buffer[1],
                                       buffer[2]
                    );

                if(midici.ciType == MIDICI_PE_CAPABILITYREPLY && recvPECapabilities != nullptr)
                    recvPECapabilitiesReplies(midici,
                                              buffer[0],
                                              buffer[1],
                                              buffer[2]
                    );

            }

            break;
        }
        default: {

            if (sysexPos == 13) {
                midici._peReqIdx = std::make_tuple(midici.remoteMUID,s7Byte);
                midici._reqTupleSet = true; //Used for cleanup
                //peRequestDetails[midici._peReqIdx] = peHeader();
                midici.requestId = s7Byte;
                intTemp[0]=0;
                return;
            }


            if (sysexPos == 14 || sysexPos == 15) { //header Length
                intTemp[0] += s7Byte << (7 * (sysexPos - 14));
                return;
            }

            uint16_t headerLength = intTemp[0];

            if (sysexPos == 16 && midici.numChunk == 1){
                peHeaderStr[midici._peReqIdx] = "";
            }

            if (sysexPos >= 16 && sysexPos <= 15 + headerLength) {
                uint16_t charOffset = (sysexPos - 16);
                buffer[charOffset] = s7Byte;
                peHeaderStr[midici._peReqIdx].push_back(s7Byte);


                if (sysexPos == 15 + headerLength) {

                    switch (midici.ciType) {
                        case MIDICI_PE_GET:
                            if (recvPEGetInquiry != nullptr) {
                                recvPEGetInquiry(midici, peHeaderStr[midici._peReqIdx]);
                                cleanupRequest(midici._peReqIdx);
                            }
                            break;
                        case MIDICI_PE_SETREPLY:
                            if (recvPESetReply != nullptr) {
                                recvPESetReply(midici, peHeaderStr[midici._peReqIdx]);
                                cleanupRequest(midici._peReqIdx);
                            }
                            break;
                        case MIDICI_PE_SUBREPLY:
                            if (recvPESubReply != nullptr) {
                                recvPESubReply(midici, peHeaderStr[midici._peReqIdx]);
                                cleanupRequest(midici._peReqIdx);
                            }
                            break;
                        case MIDICI_PE_NOTIFY:
                            if (recvPENotify != nullptr) {
                                recvPENotify(midici, peHeaderStr[midici._peReqIdx]);
                                cleanupRequest(midici._peReqIdx);
                            }
                            break;
                    }
                }
            }

            if (sysexPos == 16 + headerLength || sysexPos == 17 + headerLength) {
                midici.totalChunks +=
                        s7Byte << (7 * (sysexPos - 16 - headerLength));
                return;
            }

            if (sysexPos == 18 + headerLength || sysexPos == 19 + headerLength) {
                midici.numChunk += s7Byte << (7 * (sysexPos - 18 - headerLength));
                return;
            }

            if (sysexPos == 20 + headerLength) { //Body Length
                intTemp[1] = s7Byte;
                return;
            }
            if (sysexPos == 21 + headerLength) { //Body Length
                intTemp[1] += s7Byte << 7;
            }

            uint16_t bodyLength = intTemp[1];
            uint16_t initPos = 22 + headerLength;
            uint16_t charOffset = (sysexPos - initPos) % S7_BUFFERLEN;

            if (
                    (sysexPos >= initPos && sysexPos <= initPos - 1 + bodyLength)
                    || (bodyLength == 0 && sysexPos == initPos - 1)
                    ) {
                if (bodyLength != 0)buffer[charOffset] = s7Byte;

                bool lastByteOfSet = (
                        midici.numChunk == midici.totalChunks &&
                        sysexPos == initPos - 1 + bodyLength);
                bool lastByteOfChunk = (bodyLength == 0 || sysexPos == initPos - 1 + bodyLength);


                if (charOffset == S7_BUFFERLEN - 1 || lastByteOfChunk) {
                    if (midici.ciType == MIDICI_PE_GETREPLY && recvPEGetReply != nullptr) {
                        recvPEGetReply(midici, peHeaderStr[midici._peReqIdx],
                                         charOffset + 1, buffer, lastByteOfChunk, lastByteOfSet);
                    }

                    if (midici.ciType == MIDICI_PE_SUB && recvPESubInquiry != nullptr) {
                        recvPESubInquiry(midici, peHeaderStr[midici._peReqIdx],
                                         charOffset + 1, buffer, lastByteOfChunk, lastByteOfSet);
                    }

                    if (midici.ciType == MIDICI_PE_SET && recvPESetInquiry != nullptr) {
                        recvPESetInquiry(midici, peHeaderStr[midici._peReqIdx],
                                         charOffset + 1, buffer, lastByteOfChunk, lastByteOfSet);
                    }
                    midici.partialChunkCount++;
                }

                if (lastByteOfSet) {
                    cleanupRequest(midici._peReqIdx);
                }
            }
            break;
        }
    }

}

void midiCIProcessor::processPISysex(uint8_t s7Byte) {
    if(midici.ciVer == 1) return;

    switch (midici.ciType) {
        case MIDICI_PI_CAPABILITY: {
            if (sysexPos == 12 && recvPICapabilities != nullptr) {
                recvPICapabilities(midici);
            }
            break;
        }
        case MIDICI_PI_CAPABILITYREPLY: {
            if (sysexPos == 13 && recvPICapabilitiesReply != nullptr) {
                recvPICapabilitiesReply(midici,s7Byte);
            }
            break;
        }
        case MIDICI_PI_MM_REPORT_END: {
            if (sysexPos == 12 && recvPIMMReportEnd != nullptr) {
                recvPIMMReportEnd(midici);
            }
            break;
        }
        case MIDICI_PI_MM_REPORT:{
            if (sysexPos == 13) {//MDC
                buffer[0] = s7Byte;
            }
            if (sysexPos == 14) {//Bitmap of requested System Message Types
                buffer[1] = s7Byte;
            }
            if (sysexPos == 16) {//Bitmap of requested Channel Controller Message Types
                buffer[2] = s7Byte;
            }
            if (sysexPos == 17 && recvPIMMReport != nullptr){
                recvPIMMReport(midici,
                               buffer[0],
                               buffer[1],
                               buffer[2],
                               s7Byte);
            }
            break;
        }
        case MIDICI_PI_MM_REPORT_REPLY: {
            if (sysexPos == 13) {//Bitmap of requested System Message Types
                buffer[0] = s7Byte;
            }
            if (sysexPos == 15) {//Bitmap of requested Channel Controller Message Types
                buffer[1] = s7Byte;
            }
            if (sysexPos == 16 && recvPIMMReportReply != nullptr){
                recvPIMMReportReply(midici,
                                    buffer[0],
                                    buffer[1],
                                    s7Byte);
            }
            break;
        }
        default: {
            break;
        }
    }
}
