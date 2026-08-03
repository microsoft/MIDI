

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for C:\Users\Pete\AppData\Local\Temp\MidiEndpointConnection.idl-80afb8d2:
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

#ifndef __MidiEndpointConnection_h_p_h__
#define __MidiEndpointConnection_h_p_h__

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

#ifndef ____FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_FWD_DEFINED__
#define ____FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_FWD_DEFINED__
typedef interface __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin;

#endif 	/* ____FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_FWD_DEFINED__ */


#ifndef ____FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_FWD_DEFINED__
#define ____FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_FWD_DEFINED__
typedef interface __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin;

#endif 	/* ____FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_FWD_DEFINED__ */


#ifndef ____FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_FWD_DEFINED__
#define ____FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_FWD_DEFINED__
typedef interface __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin;

#endif 	/* ____FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_FWD_DEFINED__ */


#ifndef ____FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_FWD_DEFINED__
#define ____FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_FWD_DEFINED__
typedef interface __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket;

#endif 	/* ____FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_FWD_DEFINED__ */


#ifndef ____FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_FWD_DEFINED__
#define ____FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_FWD_DEFINED__
typedef interface __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket;

#endif 	/* ____FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_FWD_DEFINED__ */


#ifndef ____FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct_FWD_DEFINED__
#define ____FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct_FWD_DEFINED__
typedef interface __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct;

#endif 	/* ____FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct_FWD_DEFINED__ */


#ifndef ____FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct_FWD_DEFINED__
#define ____FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct_FWD_DEFINED__
typedef interface __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct;

#endif 	/* ____FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct_FWD_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_FWD_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics_FWD_DEFINED__ */


/* header files for imported files */
#include "Windows.Foundation.h"
#include "MidiApiContracts.h"
#include "IMidiMessageReceivedEventSource.h"
#include "IMidiEndpointMessageProcessingPlugin.h"
#include "MidiSendMessageResultsEnum.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_MidiEndpointConnection_0000_0000 */
/* [local] */ 
















#pragma once 



extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0000_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiEndpointConnection2Eidl_0000_0212 */




extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection2Eidl_0000_0212_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection2Eidl_0000_0212_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiEndpointConnection_0000_0001 */
/* [local] */ 

#ifndef DEF___FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin
#define DEF___FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin
#if !defined(__cplusplus) || defined(RO_NO_TEMPLATE_NAME)


extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0001_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0001_v0_0_s_ifspec;

#ifndef ____FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_INTERFACE_DEFINED__
#define ____FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_INTERFACE_DEFINED__

/* interface __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID___FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("a6b0ae23-20a4-50f5-a9b8-022988f35adf")
    __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin : public IInspectable
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Current( 
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointMessageProcessingPlugin **current) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_HasCurrent( 
            /* [retval][out] */ boolean *hasCurrent) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE MoveNext( 
            /* [retval][out] */ boolean *hasCurrent) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMany( 
            /* [in] */ unsigned int capacity,
            /* [size_is][length_is][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointMessageProcessingPlugin **items,
            /* [retval][out] */ unsigned int *actual) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPluginVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin, get_Current)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Current )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointMessageProcessingPlugin **current);
        
        DECLSPEC_XFGVIRT(__FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin, get_HasCurrent)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_HasCurrent )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [retval][out] */ boolean *hasCurrent);
        
        DECLSPEC_XFGVIRT(__FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin, MoveNext)
        HRESULT ( STDMETHODCALLTYPE *MoveNext )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [retval][out] */ boolean *hasCurrent);
        
        DECLSPEC_XFGVIRT(__FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin, GetMany)
        HRESULT ( STDMETHODCALLTYPE *GetMany )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [in] */ unsigned int capacity,
            /* [size_is][length_is][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointMessageProcessingPlugin **items,
            /* [retval][out] */ unsigned int *actual);
        
        END_INTERFACE
    } __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPluginVtbl;

    interface __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin
    {
        CONST_VTBL struct __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPluginVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_get_Current(This,current)	\
    ( (This)->lpVtbl -> get_Current(This,current) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_get_HasCurrent(This,hasCurrent)	\
    ( (This)->lpVtbl -> get_HasCurrent(This,hasCurrent) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_MoveNext(This,hasCurrent)	\
    ( (This)->lpVtbl -> MoveNext(This,hasCurrent) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_GetMany(This,capacity,items,actual)	\
    ( (This)->lpVtbl -> GetMany(This,capacity,items,actual) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_MidiEndpointConnection_0000_0002 */
/* [local] */ 

#endif /* pinterface */
#endif /* DEF___FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin */


extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0002_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0002_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiEndpointConnection2Eidl_0000_0213 */




extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection2Eidl_0000_0213_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection2Eidl_0000_0213_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiEndpointConnection_0000_0003 */
/* [local] */ 

#ifndef DEF___FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin
#define DEF___FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin
#if !defined(__cplusplus) || defined(RO_NO_TEMPLATE_NAME)


extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0003_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0003_v0_0_s_ifspec;

#ifndef ____FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_INTERFACE_DEFINED__
#define ____FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_INTERFACE_DEFINED__

/* interface __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID___FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("31f2c25f-eda5-501d-9549-1262ef31a26f")
    __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin : public IInspectable
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE First( 
            /* [retval][out] */ __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin **first) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPluginVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin, First)
        HRESULT ( STDMETHODCALLTYPE *First )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [retval][out] */ __FIIterator_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin **first);
        
        END_INTERFACE
    } __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPluginVtbl;

    interface __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin
    {
        CONST_VTBL struct __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPluginVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_First(This,first)	\
    ( (This)->lpVtbl -> First(This,first) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_MidiEndpointConnection_0000_0004 */
/* [local] */ 

#endif /* pinterface */
#endif /* DEF___FIIterable_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin */


extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0004_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0004_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiEndpointConnection2Eidl_0000_0214 */




extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection2Eidl_0000_0214_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection2Eidl_0000_0214_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiEndpointConnection_0000_0005 */
/* [local] */ 

#ifndef DEF___FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin
#define DEF___FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin
#if !defined(__cplusplus) || defined(RO_NO_TEMPLATE_NAME)


extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0005_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0005_v0_0_s_ifspec;

#ifndef ____FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_INTERFACE_DEFINED__
#define ____FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_INTERFACE_DEFINED__

/* interface __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID___FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("4c045b70-0f74-597f-98b5-93f7437a7b29")
    __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin : public IInspectable
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetAt( 
            /* [in] */ unsigned int index,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointMessageProcessingPlugin **item) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Size( 
            /* [retval][out] */ unsigned int *size) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE IndexOf( 
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointMessageProcessingPlugin *item,
            /* [out] */ unsigned int *index,
            /* [retval][out] */ boolean *found) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMany( 
            /* [in] */ unsigned int startIndex,
            /* [in] */ unsigned int capacity,
            /* [size_is][length_is][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointMessageProcessingPlugin **items,
            /* [retval][out] */ unsigned int *actual) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPluginVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin, GetAt)
        HRESULT ( STDMETHODCALLTYPE *GetAt )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [in] */ unsigned int index,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointMessageProcessingPlugin **item);
        
        DECLSPEC_XFGVIRT(__FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin, get_Size)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Size )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [retval][out] */ unsigned int *size);
        
        DECLSPEC_XFGVIRT(__FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin, IndexOf)
        HRESULT ( STDMETHODCALLTYPE *IndexOf )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointMessageProcessingPlugin *item,
            /* [out] */ unsigned int *index,
            /* [retval][out] */ boolean *found);
        
        DECLSPEC_XFGVIRT(__FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin, GetMany)
        HRESULT ( STDMETHODCALLTYPE *GetMany )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin * This,
            /* [in] */ unsigned int startIndex,
            /* [in] */ unsigned int capacity,
            /* [size_is][length_is][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointMessageProcessingPlugin **items,
            /* [retval][out] */ unsigned int *actual);
        
        END_INTERFACE
    } __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPluginVtbl;

    interface __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin
    {
        CONST_VTBL struct __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPluginVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_GetAt(This,index,item)	\
    ( (This)->lpVtbl -> GetAt(This,index,item) ) 

#define __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_get_Size(This,size)	\
    ( (This)->lpVtbl -> get_Size(This,size) ) 

#define __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_IndexOf(This,item,index,found)	\
    ( (This)->lpVtbl -> IndexOf(This,item,index,found) ) 

#define __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_GetMany(This,startIndex,capacity,items,actual)	\
    ( (This)->lpVtbl -> GetMany(This,startIndex,capacity,items,actual) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_MidiEndpointConnection_0000_0006 */
/* [local] */ 

#endif /* pinterface */
#endif /* DEF___FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin */


extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0006_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0006_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiEndpointConnection2Eidl_0000_0215 */




extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection2Eidl_0000_0215_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection2Eidl_0000_0215_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiEndpointConnection_0000_0007 */
/* [local] */ 

#ifndef DEF___FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket
#define DEF___FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket
#if !defined(__cplusplus) || defined(RO_NO_TEMPLATE_NAME)


extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0007_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0007_v0_0_s_ifspec;

#ifndef ____FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_INTERFACE_DEFINED__
#define ____FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_INTERFACE_DEFINED__

/* interface __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID___FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("a578fd89-feb0-5da3-9856-5981772caa9b")
    __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket : public IInspectable
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Current( 
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket **current) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_HasCurrent( 
            /* [retval][out] */ boolean *hasCurrent) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE MoveNext( 
            /* [retval][out] */ boolean *hasCurrent) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMany( 
            /* [in] */ unsigned int capacity,
            /* [size_is][length_is][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket **items,
            /* [retval][out] */ unsigned int *actual) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacketVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket, get_Current)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Current )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket * This,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket **current);
        
        DECLSPEC_XFGVIRT(__FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket, get_HasCurrent)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_HasCurrent )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket * This,
            /* [retval][out] */ boolean *hasCurrent);
        
        DECLSPEC_XFGVIRT(__FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket, MoveNext)
        HRESULT ( STDMETHODCALLTYPE *MoveNext )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket * This,
            /* [retval][out] */ boolean *hasCurrent);
        
        DECLSPEC_XFGVIRT(__FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket, GetMany)
        HRESULT ( STDMETHODCALLTYPE *GetMany )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket * This,
            /* [in] */ unsigned int capacity,
            /* [size_is][length_is][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket **items,
            /* [retval][out] */ unsigned int *actual);
        
        END_INTERFACE
    } __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacketVtbl;

    interface __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket
    {
        CONST_VTBL struct __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacketVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_get_Current(This,current)	\
    ( (This)->lpVtbl -> get_Current(This,current) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_get_HasCurrent(This,hasCurrent)	\
    ( (This)->lpVtbl -> get_HasCurrent(This,hasCurrent) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_MoveNext(This,hasCurrent)	\
    ( (This)->lpVtbl -> MoveNext(This,hasCurrent) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_GetMany(This,capacity,items,actual)	\
    ( (This)->lpVtbl -> GetMany(This,capacity,items,actual) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_MidiEndpointConnection_0000_0008 */
/* [local] */ 

#endif /* pinterface */
#endif /* DEF___FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket */


extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0008_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0008_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiEndpointConnection2Eidl_0000_0216 */




extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection2Eidl_0000_0216_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection2Eidl_0000_0216_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiEndpointConnection_0000_0009 */
/* [local] */ 

#ifndef DEF___FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket
#define DEF___FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket
#if !defined(__cplusplus) || defined(RO_NO_TEMPLATE_NAME)


extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0009_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0009_v0_0_s_ifspec;

#ifndef ____FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_INTERFACE_DEFINED__
#define ____FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_INTERFACE_DEFINED__

/* interface __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID___FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("e58bbd8d-4c47-5b78-bf1e-46208e85dc24")
    __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket : public IInspectable
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE First( 
            /* [retval][out] */ __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket **first) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacketVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket, First)
        HRESULT ( STDMETHODCALLTYPE *First )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket * This,
            /* [retval][out] */ __FIIterator_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket **first);
        
        END_INTERFACE
    } __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacketVtbl;

    interface __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket
    {
        CONST_VTBL struct __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacketVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_First(This,first)	\
    ( (This)->lpVtbl -> First(This,first) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_MidiEndpointConnection_0000_0010 */
/* [local] */ 

#endif /* pinterface */
#endif /* DEF___FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket */


extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0010_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0010_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiEndpointConnection2Eidl_0000_0217 */




extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection2Eidl_0000_0217_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection2Eidl_0000_0217_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiEndpointConnection_0000_0011 */
/* [local] */ 

#ifndef DEF___FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct
#define DEF___FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct
#if !defined(__cplusplus) || defined(RO_NO_TEMPLATE_NAME)


extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0011_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0011_v0_0_s_ifspec;

#ifndef ____FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct_INTERFACE_DEFINED__
#define ____FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct_INTERFACE_DEFINED__

/* interface __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID___FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8e7b9609-9325-5575-a797-69b188991698")
    __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct : public IInspectable
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Current( 
            /* [retval][out] */ struct __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageStruct *current) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_HasCurrent( 
            /* [retval][out] */ boolean *hasCurrent) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE MoveNext( 
            /* [retval][out] */ boolean *hasCurrent) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMany( 
            /* [in] */ unsigned int capacity,
            /* [size_is][length_is][out] */ struct __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageStruct *items,
            /* [retval][out] */ unsigned int *actual) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStructVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct, get_Current)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Current )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct * This,
            /* [retval][out] */ struct __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageStruct *current);
        
        DECLSPEC_XFGVIRT(__FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct, get_HasCurrent)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_HasCurrent )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct * This,
            /* [retval][out] */ boolean *hasCurrent);
        
        DECLSPEC_XFGVIRT(__FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct, MoveNext)
        HRESULT ( STDMETHODCALLTYPE *MoveNext )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct * This,
            /* [retval][out] */ boolean *hasCurrent);
        
        DECLSPEC_XFGVIRT(__FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct, GetMany)
        HRESULT ( STDMETHODCALLTYPE *GetMany )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct * This,
            /* [in] */ unsigned int capacity,
            /* [size_is][length_is][out] */ struct __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageStruct *items,
            /* [retval][out] */ unsigned int *actual);
        
        END_INTERFACE
    } __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStructVtbl;

    interface __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct
    {
        CONST_VTBL struct __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStructVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct_get_Current(This,current)	\
    ( (This)->lpVtbl -> get_Current(This,current) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct_get_HasCurrent(This,hasCurrent)	\
    ( (This)->lpVtbl -> get_HasCurrent(This,hasCurrent) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct_MoveNext(This,hasCurrent)	\
    ( (This)->lpVtbl -> MoveNext(This,hasCurrent) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct_GetMany(This,capacity,items,actual)	\
    ( (This)->lpVtbl -> GetMany(This,capacity,items,actual) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_MidiEndpointConnection_0000_0012 */
/* [local] */ 

#endif /* pinterface */
#endif /* DEF___FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct */


extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0012_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0012_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiEndpointConnection2Eidl_0000_0218 */




extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection2Eidl_0000_0218_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection2Eidl_0000_0218_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiEndpointConnection_0000_0013 */
/* [local] */ 

#ifndef DEF___FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct
#define DEF___FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct
#if !defined(__cplusplus) || defined(RO_NO_TEMPLATE_NAME)


extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0013_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0013_v0_0_s_ifspec;

#ifndef ____FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct_INTERFACE_DEFINED__
#define ____FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct_INTERFACE_DEFINED__

/* interface __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID___FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("b9fe7000-a725-5c2d-ad18-263ab687f552")
    __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct : public IInspectable
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE First( 
            /* [retval][out] */ __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct **first) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStructVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct, First)
        HRESULT ( STDMETHODCALLTYPE *First )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct * This,
            /* [retval][out] */ __FIIterator_1_Windows__CDevices__CMidi2__CMidiMessageStruct **first);
        
        END_INTERFACE
    } __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStructVtbl;

    interface __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct
    {
        CONST_VTBL struct __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStructVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct_First(This,first)	\
    ( (This)->lpVtbl -> First(This,first) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_MidiEndpointConnection_0000_0014 */
/* [local] */ 

#endif /* pinterface */
#endif /* DEF___FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct */


extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0014_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnection_0000_0014_v0_0_s_ifspec;

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-dd0010005000")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection : public IInspectable
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_LogMessageDataValidationErrorDetails( 
            /* [retval][out] */ boolean *value) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_LogMessageDataValidationErrorDetails( 
            /* [in] */ boolean value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Open( 
            /* [retval][out] */ boolean *result) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_MessageProcessingPlugins( 
            /* [retval][out] */ __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin **value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE AddMessageProcessingPlugin( 
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointMessageProcessingPlugin *plugin) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE RemoveMessageProcessingPlugin( 
            /* [in] */ GUID id) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendSingleMessagePacket( 
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket *message,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendSingleMessageStruct( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ byte wordCount,
            /* [in] */ const struct __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageStruct *message,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendSingleMessageWordArray( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int startIndex,
            /* [in] */ byte wordCount,
            /* [in] */ unsigned int wordsLength,
            /* [in][size_is] */ unsigned int *words,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendSingleMessageWords( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int word0,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendSingleMessageWords2( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int word0,
            /* [in] */ unsigned int word1,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendSingleMessageWords3( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int word0,
            /* [in] */ unsigned int word1,
            /* [in] */ unsigned int word2,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendSingleMessageWords4( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int word0,
            /* [in] */ unsigned int word1,
            /* [in] */ unsigned int word2,
            /* [in] */ unsigned int word3,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendSingleMessageBuffer( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int byteOffset,
            /* [in] */ byte byteCount,
            /* [in] */ __x_ABI_CWindows_CFoundation_CIMemoryBuffer *buffer,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendMultipleMessagesWordList( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ __FIIterable_1_UINT32 *words,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendMultipleMessagesWordArray( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int startIndex,
            /* [in] */ unsigned int wordCount,
            /* [in] */ unsigned int wordsLength,
            /* [in][size_is] */ unsigned int *words,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendMultipleMessagesPacketList( 
            /* [in] */ __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket *messages,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendMultipleMessagesStructList( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct *messages,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendMultipleMessagesStructArray( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int startIndex,
            /* [in] */ unsigned int messageCount,
            /* [in] */ unsigned int messagesLength,
            /* [in][size_is] */ struct __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageStruct *messages,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendMultipleMessagesBuffer( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int byteOffset,
            /* [in] */ unsigned int byteCount,
            /* [in] */ __x_ABI_CWindows_CFoundation_CIMemoryBuffer *buffer,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetSupportedMaxMidiWordsPerTransmission( 
            /* [retval][out] */ unsigned int *result) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, get_LogMessageDataValidationErrorDetails)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_LogMessageDataValidationErrorDetails )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [retval][out] */ boolean *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, put_LogMessageDataValidationErrorDetails)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_LogMessageDataValidationErrorDetails )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [in] */ boolean value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, Open)
        HRESULT ( STDMETHODCALLTYPE *Open )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [retval][out] */ boolean *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, get_MessageProcessingPlugins)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_MessageProcessingPlugins )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [retval][out] */ __FIVectorView_1_Windows__CDevices__CMidi2__CIMidiEndpointMessageProcessingPlugin **value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, AddMessageProcessingPlugin)
        HRESULT ( STDMETHODCALLTYPE *AddMessageProcessingPlugin )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointMessageProcessingPlugin *plugin);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, RemoveMessageProcessingPlugin)
        HRESULT ( STDMETHODCALLTYPE *RemoveMessageProcessingPlugin )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [in] */ GUID id);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, SendSingleMessagePacket)
        HRESULT ( STDMETHODCALLTYPE *SendSingleMessagePacket )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalPacket *message,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, SendSingleMessageStruct)
        HRESULT ( STDMETHODCALLTYPE *SendSingleMessageStruct )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ byte wordCount,
            /* [in] */ const struct __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageStruct *message,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, SendSingleMessageWordArray)
        HRESULT ( STDMETHODCALLTYPE *SendSingleMessageWordArray )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int startIndex,
            /* [in] */ byte wordCount,
            /* [in] */ unsigned int wordsLength,
            /* [in][size_is] */ unsigned int *words,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, SendSingleMessageWords)
        HRESULT ( STDMETHODCALLTYPE *SendSingleMessageWords )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int word0,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, SendSingleMessageWords2)
        HRESULT ( STDMETHODCALLTYPE *SendSingleMessageWords2 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int word0,
            /* [in] */ unsigned int word1,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, SendSingleMessageWords3)
        HRESULT ( STDMETHODCALLTYPE *SendSingleMessageWords3 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int word0,
            /* [in] */ unsigned int word1,
            /* [in] */ unsigned int word2,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, SendSingleMessageWords4)
        HRESULT ( STDMETHODCALLTYPE *SendSingleMessageWords4 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int word0,
            /* [in] */ unsigned int word1,
            /* [in] */ unsigned int word2,
            /* [in] */ unsigned int word3,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, SendSingleMessageBuffer)
        HRESULT ( STDMETHODCALLTYPE *SendSingleMessageBuffer )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int byteOffset,
            /* [in] */ byte byteCount,
            /* [in] */ __x_ABI_CWindows_CFoundation_CIMemoryBuffer *buffer,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, SendMultipleMessagesWordList)
        HRESULT ( STDMETHODCALLTYPE *SendMultipleMessagesWordList )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ __FIIterable_1_UINT32 *words,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, SendMultipleMessagesWordArray)
        HRESULT ( STDMETHODCALLTYPE *SendMultipleMessagesWordArray )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int startIndex,
            /* [in] */ unsigned int wordCount,
            /* [in] */ unsigned int wordsLength,
            /* [in][size_is] */ unsigned int *words,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, SendMultipleMessagesPacketList)
        HRESULT ( STDMETHODCALLTYPE *SendMultipleMessagesPacketList )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [in] */ __FIIterable_1_Windows__CDevices__CMidi2__CIMidiUniversalPacket *messages,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, SendMultipleMessagesStructList)
        HRESULT ( STDMETHODCALLTYPE *SendMultipleMessagesStructList )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ __FIIterable_1_Windows__CDevices__CMidi2__CMidiMessageStruct *messages,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, SendMultipleMessagesStructArray)
        HRESULT ( STDMETHODCALLTYPE *SendMultipleMessagesStructArray )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int startIndex,
            /* [in] */ unsigned int messageCount,
            /* [in] */ unsigned int messagesLength,
            /* [in][size_is] */ struct __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageStruct *messages,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, SendMultipleMessagesBuffer)
        HRESULT ( STDMETHODCALLTYPE *SendMultipleMessagesBuffer )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int byteOffset,
            /* [in] */ unsigned int byteCount,
            /* [in] */ __x_ABI_CWindows_CFoundation_CIMemoryBuffer *buffer,
            /* [retval][out] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection, GetSupportedMaxMidiWordsPerTransmission)
        HRESULT ( STDMETHODCALLTYPE *GetSupportedMaxMidiWordsPerTransmission )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection * This,
            /* [retval][out] */ unsigned int *result);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_get_LogMessageDataValidationErrorDetails(This,value)	\
    ( (This)->lpVtbl -> get_LogMessageDataValidationErrorDetails(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_put_LogMessageDataValidationErrorDetails(This,value)	\
    ( (This)->lpVtbl -> put_LogMessageDataValidationErrorDetails(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_Open(This,result)	\
    ( (This)->lpVtbl -> Open(This,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_get_MessageProcessingPlugins(This,value)	\
    ( (This)->lpVtbl -> get_MessageProcessingPlugins(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_AddMessageProcessingPlugin(This,plugin)	\
    ( (This)->lpVtbl -> AddMessageProcessingPlugin(This,plugin) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_RemoveMessageProcessingPlugin(This,id)	\
    ( (This)->lpVtbl -> RemoveMessageProcessingPlugin(This,id) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_SendSingleMessagePacket(This,message,result)	\
    ( (This)->lpVtbl -> SendSingleMessagePacket(This,message,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_SendSingleMessageStruct(This,timestamp,wordCount,message,result)	\
    ( (This)->lpVtbl -> SendSingleMessageStruct(This,timestamp,wordCount,message,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_SendSingleMessageWordArray(This,timestamp,startIndex,wordCount,wordsLength,words,result)	\
    ( (This)->lpVtbl -> SendSingleMessageWordArray(This,timestamp,startIndex,wordCount,wordsLength,words,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_SendSingleMessageWords(This,timestamp,word0,result)	\
    ( (This)->lpVtbl -> SendSingleMessageWords(This,timestamp,word0,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_SendSingleMessageWords2(This,timestamp,word0,word1,result)	\
    ( (This)->lpVtbl -> SendSingleMessageWords2(This,timestamp,word0,word1,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_SendSingleMessageWords3(This,timestamp,word0,word1,word2,result)	\
    ( (This)->lpVtbl -> SendSingleMessageWords3(This,timestamp,word0,word1,word2,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_SendSingleMessageWords4(This,timestamp,word0,word1,word2,word3,result)	\
    ( (This)->lpVtbl -> SendSingleMessageWords4(This,timestamp,word0,word1,word2,word3,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_SendSingleMessageBuffer(This,timestamp,byteOffset,byteCount,buffer,result)	\
    ( (This)->lpVtbl -> SendSingleMessageBuffer(This,timestamp,byteOffset,byteCount,buffer,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_SendMultipleMessagesWordList(This,timestamp,words,result)	\
    ( (This)->lpVtbl -> SendMultipleMessagesWordList(This,timestamp,words,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_SendMultipleMessagesWordArray(This,timestamp,startIndex,wordCount,wordsLength,words,result)	\
    ( (This)->lpVtbl -> SendMultipleMessagesWordArray(This,timestamp,startIndex,wordCount,wordsLength,words,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_SendMultipleMessagesPacketList(This,messages,result)	\
    ( (This)->lpVtbl -> SendMultipleMessagesPacketList(This,messages,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_SendMultipleMessagesStructList(This,timestamp,messages,result)	\
    ( (This)->lpVtbl -> SendMultipleMessagesStructList(This,timestamp,messages,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_SendMultipleMessagesStructArray(This,timestamp,startIndex,messageCount,messagesLength,messages,result)	\
    ( (This)->lpVtbl -> SendMultipleMessagesStructArray(This,timestamp,startIndex,messageCount,messagesLength,messages,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_SendMultipleMessagesBuffer(This,timestamp,byteOffset,byteCount,buffer,result)	\
    ( (This)->lpVtbl -> SendMultipleMessagesBuffer(This,timestamp,byteOffset,byteCount,buffer,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_GetSupportedMaxMidiWordsPerTransmission(This,result)	\
    ( (This)->lpVtbl -> GetSupportedMaxMidiWordsPerTransmission(This,result) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnection_INTERFACE_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-ee0010005000")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics : public IInspectable
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetDeviceSelector( 
            /* [retval][out] */ HSTRING *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendMessageSucceeded( 
            /* [in] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults sendResult,
            /* [retval][out] */ boolean *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendMessageFailed( 
            /* [in] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults sendResult,
            /* [retval][out] */ boolean *result) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStaticsVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics, GetDeviceSelector)
        HRESULT ( STDMETHODCALLTYPE *GetDeviceSelector )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics * This,
            /* [retval][out] */ HSTRING *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics, SendMessageSucceeded)
        HRESULT ( STDMETHODCALLTYPE *SendMessageSucceeded )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics * This,
            /* [in] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults sendResult,
            /* [retval][out] */ boolean *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics, SendMessageFailed)
        HRESULT ( STDMETHODCALLTYPE *SendMessageFailed )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics * This,
            /* [in] */ enum __x_ABI_CWindows_CDevices_CMidi2_CMidiSendMessageResults sendResult,
            /* [retval][out] */ boolean *result);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStaticsVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStaticsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics_GetDeviceSelector(This,result)	\
    ( (This)->lpVtbl -> GetDeviceSelector(This,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics_SendMessageSucceeded(This,sendResult,result)	\
    ( (This)->lpVtbl -> SendMessageSucceeded(This,sendResult,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics_SendMessageFailed(This,sendResult,result)	\
    ( (This)->lpVtbl -> SendMessageFailed(This,sendResult,result) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionStatics_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

unsigned long             __RPC_USER  HSTRING_UserSize(     unsigned long *, unsigned long            , HSTRING * ); 
unsigned char * __RPC_USER  HSTRING_UserMarshal(  unsigned long *, unsigned char *, HSTRING * ); 
unsigned char * __RPC_USER  HSTRING_UserUnmarshal(unsigned long *, unsigned char *, HSTRING * ); 
void                      __RPC_USER  HSTRING_UserFree(     unsigned long *, HSTRING * ); 

unsigned long             __RPC_USER  HSTRING_UserSize64(     unsigned long *, unsigned long            , HSTRING * ); 
unsigned char * __RPC_USER  HSTRING_UserMarshal64(  unsigned long *, unsigned char *, HSTRING * ); 
unsigned char * __RPC_USER  HSTRING_UserUnmarshal64(unsigned long *, unsigned char *, HSTRING * ); 
void                      __RPC_USER  HSTRING_UserFree64(     unsigned long *, HSTRING * ); 

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


