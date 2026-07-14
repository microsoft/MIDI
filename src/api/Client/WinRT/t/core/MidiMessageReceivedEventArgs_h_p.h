

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for C:\Users\Pete\AppData\Local\Temp\MidiMessageReceivedEventArgs.idl-c7452e05:
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

#ifndef __MidiMessageReceivedEventArgs_h_p_h__
#define __MidiMessageReceivedEventArgs_h_p_h__

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

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_FWD_DEFINED__ */


/* header files for imported files */
#include "Windows.Foundation.h"
#include "MidiApiContracts.h"
#include "IMidiUniversalPacket.h"
#include "MidiMessage32.h"
#include "MidiMessage64.h"
#include "MidiMessage96.h"
#include "MidiMessage128.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_MidiMessageReceivedEventArgs_0000_0000 */
/* [local] */ 







#pragma once 



extern RPC_IF_HANDLE __MIDL_itf_MidiMessageReceivedEventArgs_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiMessageReceivedEventArgs_0000_0000_v0_0_s_ifspec;

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-dd0010007000")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs : public IInspectable
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Timestamp( 
            /* [retval][out] */ unsigned __int64 *value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_PacketType( 
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiPacketType *value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_MessageType( 
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageType *value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE PeekFirstWord( 
            /* [retval][out] */ unsigned int *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMessagePacket( 
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket **result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE FillWords( 
            /* [out] */ unsigned int *word0,
            /* [out] */ unsigned int *word1,
            /* [out] */ unsigned int *word2,
            /* [out] */ unsigned int *word3,
            /* [retval][out] */ byte *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE FillMessageStruct( 
            /* [out] */ struct __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageStruct *message,
            /* [retval][out] */ byte *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE FillMessage32( 
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage32 *message,
            /* [retval][out] */ boolean *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE FillMessage64( 
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 *message,
            /* [retval][out] */ boolean *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE FillMessage96( 
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 *message,
            /* [retval][out] */ boolean *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE FillMessage128( 
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 *message,
            /* [retval][out] */ boolean *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE FillWordArray( 
            /* [in] */ unsigned int startIndex,
            /* [in] */ unsigned int wordsLength,
            /* [out][size_is] */ unsigned int *words,
            /* [retval][out] */ byte *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE FillByteArray( 
            /* [in] */ unsigned int startIndex,
            /* [in] */ unsigned int bytesLength,
            /* [out][size_is] */ byte *bytes,
            /* [retval][out] */ byte *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE FillBuffer( 
            /* [in] */ unsigned int byteOffset,
            /* [in] */ __x_ABI_CWindows_CFoundation_CIMemoryBuffer *buffer,
            /* [retval][out] */ byte *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE AppendWordsToList( 
            /* [in] */ __FIVector_1_UINT32 *wordList,
            /* [retval][out] */ byte *result) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgsVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs, get_Timestamp)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Timestamp )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [retval][out] */ unsigned __int64 *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs, get_PacketType)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_PacketType )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiPacketType *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs, get_MessageType)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_MessageType )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageType *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs, PeekFirstWord)
        HRESULT ( STDMETHODCALLTYPE *PeekFirstWord )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [retval][out] */ unsigned int *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs, GetMessagePacket)
        HRESULT ( STDMETHODCALLTYPE *GetMessagePacket )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket **result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs, FillWords)
        HRESULT ( STDMETHODCALLTYPE *FillWords )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [out] */ unsigned int *word0,
            /* [out] */ unsigned int *word1,
            /* [out] */ unsigned int *word2,
            /* [out] */ unsigned int *word3,
            /* [retval][out] */ byte *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs, FillMessageStruct)
        HRESULT ( STDMETHODCALLTYPE *FillMessageStruct )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [out] */ struct __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageStruct *message,
            /* [retval][out] */ byte *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs, FillMessage32)
        HRESULT ( STDMETHODCALLTYPE *FillMessage32 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage32 *message,
            /* [retval][out] */ boolean *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs, FillMessage64)
        HRESULT ( STDMETHODCALLTYPE *FillMessage64 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 *message,
            /* [retval][out] */ boolean *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs, FillMessage96)
        HRESULT ( STDMETHODCALLTYPE *FillMessage96 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 *message,
            /* [retval][out] */ boolean *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs, FillMessage128)
        HRESULT ( STDMETHODCALLTYPE *FillMessage128 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 *message,
            /* [retval][out] */ boolean *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs, FillWordArray)
        HRESULT ( STDMETHODCALLTYPE *FillWordArray )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [in] */ unsigned int startIndex,
            /* [in] */ unsigned int wordsLength,
            /* [out][size_is] */ unsigned int *words,
            /* [retval][out] */ byte *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs, FillByteArray)
        HRESULT ( STDMETHODCALLTYPE *FillByteArray )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [in] */ unsigned int startIndex,
            /* [in] */ unsigned int bytesLength,
            /* [out][size_is] */ byte *bytes,
            /* [retval][out] */ byte *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs, FillBuffer)
        HRESULT ( STDMETHODCALLTYPE *FillBuffer )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [in] */ unsigned int byteOffset,
            /* [in] */ __x_ABI_CWindows_CFoundation_CIMemoryBuffer *buffer,
            /* [retval][out] */ byte *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs, AppendWordsToList)
        HRESULT ( STDMETHODCALLTYPE *AppendWordsToList )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs * This,
            /* [in] */ __FIVector_1_UINT32 *wordList,
            /* [retval][out] */ byte *result);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgsVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_get_Timestamp(This,value)	\
    ( (This)->lpVtbl -> get_Timestamp(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_get_PacketType(This,value)	\
    ( (This)->lpVtbl -> get_PacketType(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_get_MessageType(This,value)	\
    ( (This)->lpVtbl -> get_MessageType(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_PeekFirstWord(This,result)	\
    ( (This)->lpVtbl -> PeekFirstWord(This,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_GetMessagePacket(This,result)	\
    ( (This)->lpVtbl -> GetMessagePacket(This,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_FillWords(This,word0,word1,word2,word3,result)	\
    ( (This)->lpVtbl -> FillWords(This,word0,word1,word2,word3,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_FillMessageStruct(This,message,result)	\
    ( (This)->lpVtbl -> FillMessageStruct(This,message,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_FillMessage32(This,message,result)	\
    ( (This)->lpVtbl -> FillMessage32(This,message,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_FillMessage64(This,message,result)	\
    ( (This)->lpVtbl -> FillMessage64(This,message,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_FillMessage96(This,message,result)	\
    ( (This)->lpVtbl -> FillMessage96(This,message,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_FillMessage128(This,message,result)	\
    ( (This)->lpVtbl -> FillMessage128(This,message,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_FillWordArray(This,startIndex,wordsLength,words,result)	\
    ( (This)->lpVtbl -> FillWordArray(This,startIndex,wordsLength,words,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_FillByteArray(This,startIndex,bytesLength,bytes,result)	\
    ( (This)->lpVtbl -> FillByteArray(This,startIndex,bytesLength,bytes,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_FillBuffer(This,byteOffset,buffer,result)	\
    ( (This)->lpVtbl -> FillBuffer(This,byteOffset,buffer,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_AppendWordsToList(This,wordList,result)	\
    ( (This)->lpVtbl -> AppendWordsToList(This,wordList,result) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


