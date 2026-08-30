

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for C:\Users\Pete\AppData\Local\Temp\MidiSendMessageResultsEnum.idl-3acc1931:
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


#ifndef __MidiSendMessageResultsEnum_h_p_h__
#define __MidiSendMessageResultsEnum_h_p_h__

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


/* interface __MIDL_itf_MidiSendMessageResultsEnum_0000_0000 */
/* [local] */ 


#pragma once 
/* [v1_enum] */ 
enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults
    {
        MidiSendMessageResults_Succeeded	= 0x80000000,
        MidiSendMessageResults_Failed	= 0x10000000,
        MidiSendMessageResults_BufferFull	= 0x10000,
        MidiSendMessageResults_EndpointConnectionClosedOrInvalid	= 0x40000,
        MidiSendMessageResults_InvalidMessageTypeForWordCount	= 0x100000,
        MidiSendMessageResults_InvalidMessageOther	= 0x200000,
        MidiSendMessageResults_DataIndexOutOfRange	= 0x400000,
        MidiSendMessageResults_TimestampOutOfRange	= 0x800000,
        MidiSendMessageResults_TransmissionWordCountExceeded	= 0x1000000,
        MidiSendMessageResults_MessageListPartiallyProcessed	= 0xf00000
    } ;


extern RPC_IF_HANDLE __MIDL_itf_MidiSendMessageResultsEnum_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiSendMessageResultsEnum_0000_0000_v0_0_s_ifspec;

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


