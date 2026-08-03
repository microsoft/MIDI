

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for C:\Users\Pete\AppData\Local\Temp\MidiMessageTypeEnum.idl-46fe7285:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.01.0628 
    protocol : all , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */



/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 500
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */


#ifndef __MidiMessageTypeEnum_h_p_h__
#define __MidiMessageTypeEnum_h_p_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifndef DECLSPEC_XFGVIRT
#if defined(_CONTROL_FLOW_GUARD_XFG)
#define DECLSPEC_XFGVIRT(base, func) __declspec(xfg_virtual(base, func))
#else
#define DECLSPEC_XFGVIRT(base, func)
#endif
#endif

#if defined(__cplusplus)
#if defined(__MIDL_USE_C_ENUM)
#define MIDL_ENUM enum
#else
#define MIDL_ENUM enum class
#endif
#endif


/* Forward Declarations */ 

/* header files for imported files */
#include "Windows.Foundation.h"
#include "MidiApiContracts.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_MidiMessageTypeEnum_0000_0000 */
/* [local] */ 


#pragma once 
/* [v1_enum] */ 
enum __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageType
    {
        MidiMessageType_UtilityMessage32	= 0,
        MidiMessageType_SystemCommon32	= 0x1,
        MidiMessageType_Midi1ChannelVoice32	= 0x2,
        MidiMessageType_DataMessage64	= 0x3,
        MidiMessageType_Midi2ChannelVoice64	= 0x4,
        MidiMessageType_DataMessage128	= 0x5,
        MidiMessageType_FutureReserved632	= 0x6,
        MidiMessageType_FutureReserved732	= 0x7,
        MidiMessageType_FutureReserved864	= 0x8,
        MidiMessageType_FutureReserved964	= 0x9,
        MidiMessageType_FutureReservedA64	= 0xa,
        MidiMessageType_FutureReservedB96	= 0xb,
        MidiMessageType_FutureReservedC96	= 0xc,
        MidiMessageType_FlexData128	= 0xd,
        MidiMessageType_FutureReservedE128	= 0xe,
        MidiMessageType_Stream128	= 0xf
    } ;


extern RPC_IF_HANDLE __MIDL_itf_MidiMessageTypeEnum_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiMessageTypeEnum_0000_0000_v0_0_s_ifspec;

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


