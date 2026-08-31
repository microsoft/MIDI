

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for C:\Users\Pete\AppData\Local\Temp\IMidiUniversalPacket.idl-846ce503:
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

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __IMidiUniversalPacket_h_p_h__
#define __IMidiUniversalPacket_h_p_h__

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

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_FWD_DEFINED__ */


/* header files for imported files */
#include "Windows.Foundation.h"
#include "MidiApiContracts.h"
#include "MidiPacketTypeEnum.h"
#include "MidiMessageTypeEnum.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_IMidiUniversalPacket_0000_0000 */
/* [local] */ 







#pragma once 


extern RPC_IF_HANDLE __MIDL_itf_IMidiUniversalPacket_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_IMidiUniversalPacket_0000_0000_v0_0_s_ifspec;

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket */
/* [uuid][object] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-ff001000F010")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket : public IInspectable
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Timestamp( 
            /* [retval][out] */ unsigned __int64 *value) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_Timestamp( 
            /* [in] */ unsigned __int64 value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_MessageType( 
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageType *value) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_MessageType( 
            /* [in] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageType value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_PacketType( 
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiPacketType *value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE PeekFirstWord( 
            /* [retval][out] */ unsigned int *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetAllWords( 
            /* [retval][out] */ __FIVector_1_UINT32 **result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE AppendAllMessageWordsToList( 
            /* [in] */ __FIVector_1_UINT32 *targetList,
            /* [retval][out] */ byte *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE FillBuffer( 
            /* [in] */ unsigned int byteOffset,
            /* [in] */ __x_ABI_CWindows_CFoundation_CIMemoryBuffer *buffer,
            /* [retval][out] */ byte *result) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacketVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket, get_Timestamp)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Timestamp )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket * This,
            /* [retval][out] */ unsigned __int64 *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket, put_Timestamp)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_Timestamp )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket * This,
            /* [in] */ unsigned __int64 value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket, get_MessageType)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_MessageType )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket * This,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageType *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket, put_MessageType)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_MessageType )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket * This,
            /* [in] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageType value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket, get_PacketType)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_PacketType )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket * This,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiPacketType *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket, PeekFirstWord)
        HRESULT ( STDMETHODCALLTYPE *PeekFirstWord )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket * This,
            /* [retval][out] */ unsigned int *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket, GetAllWords)
        HRESULT ( STDMETHODCALLTYPE *GetAllWords )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket * This,
            /* [retval][out] */ __FIVector_1_UINT32 **result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket, AppendAllMessageWordsToList)
        HRESULT ( STDMETHODCALLTYPE *AppendAllMessageWordsToList )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket * This,
            /* [in] */ __FIVector_1_UINT32 *targetList,
            /* [retval][out] */ byte *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket, FillBuffer)
        HRESULT ( STDMETHODCALLTYPE *FillBuffer )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket * This,
            /* [in] */ unsigned int byteOffset,
            /* [in] */ __x_ABI_CWindows_CFoundation_CIMemoryBuffer *buffer,
            /* [retval][out] */ byte *result);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacketVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacketVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_get_Timestamp(This,value)	\
    ( (This)->lpVtbl -> get_Timestamp(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_put_Timestamp(This,value)	\
    ( (This)->lpVtbl -> put_Timestamp(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_get_MessageType(This,value)	\
    ( (This)->lpVtbl -> get_MessageType(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_put_MessageType(This,value)	\
    ( (This)->lpVtbl -> put_MessageType(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_get_PacketType(This,value)	\
    ( (This)->lpVtbl -> get_PacketType(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_PeekFirstWord(This,result)	\
    ( (This)->lpVtbl -> PeekFirstWord(This,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_GetAllWords(This,result)	\
    ( (This)->lpVtbl -> GetAllWords(This,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_AppendAllMessageWordsToList(This,targetList,result)	\
    ( (This)->lpVtbl -> AppendAllMessageWordsToList(This,targetList,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_FillBuffer(This,byteOffset,buffer,result)	\
    ( (This)->lpVtbl -> FillBuffer(This,byteOffset,buffer,result) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


