

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for C:\Users\Pete\AppData\Local\Temp\MidiServicePingResponseSummary.idl-840ab057:
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

#ifndef __MidiServicePingResponseSummary_h_p_h__
#define __MidiServicePingResponseSummary_h_p_h__

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

#ifndef ____FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_FWD_DEFINED__
#define ____FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_FWD_DEFINED__
typedef interface __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse;

#endif 	/* ____FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_FWD_DEFINED__ */


#ifndef ____FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_FWD_DEFINED__
#define ____FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_FWD_DEFINED__
typedef interface __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse;

#endif 	/* ____FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_FWD_DEFINED__ */


#ifndef ____FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_FWD_DEFINED__
#define ____FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_FWD_DEFINED__
typedef interface __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse;

#endif 	/* ____FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_FWD_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary_FWD_DEFINED__ */


/* header files for imported files */
#include "Windows.Foundation.h"
#include "MidiApiContracts.h"
#include "MidiServicePingResponse.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_MidiServicePingResponseSummary_0000_0000 */
/* [local] */ 







#pragma once 



extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary_0000_0000_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiServicePingResponseSummary2Eidl_0000_0198 */




extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary2Eidl_0000_0198_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary2Eidl_0000_0198_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiServicePingResponseSummary_0000_0001 */
/* [local] */ 

#ifndef DEF___FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse
#define DEF___FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse
#if !defined(__cplusplus) || defined(RO_NO_TEMPLATE_NAME)


extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary_0000_0001_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary_0000_0001_v0_0_s_ifspec;

#ifndef ____FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_INTERFACE_DEFINED__
#define ____FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_INTERFACE_DEFINED__

/* interface __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID___FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("148aa2ca-e6d1-57a0-91a3-777efd4019e9")
    __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse : public IInspectable
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Current( 
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponse **current) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_HasCurrent( 
            /* [retval][out] */ boolean *hasCurrent) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE MoveNext( 
            /* [retval][out] */ boolean *hasCurrent) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMany( 
            /* [in] */ unsigned int capacity,
            /* [size_is][length_is][out] */ __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponse **items,
            /* [retval][out] */ unsigned int *actual) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponseVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse, get_Current)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Current )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponse **current);
        
        DECLSPEC_XFGVIRT(__FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse, get_HasCurrent)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_HasCurrent )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [retval][out] */ boolean *hasCurrent);
        
        DECLSPEC_XFGVIRT(__FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse, MoveNext)
        HRESULT ( STDMETHODCALLTYPE *MoveNext )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [retval][out] */ boolean *hasCurrent);
        
        DECLSPEC_XFGVIRT(__FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse, GetMany)
        HRESULT ( STDMETHODCALLTYPE *GetMany )( 
            __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [in] */ unsigned int capacity,
            /* [size_is][length_is][out] */ __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponse **items,
            /* [retval][out] */ unsigned int *actual);
        
        END_INTERFACE
    } __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponseVtbl;

    interface __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse
    {
        CONST_VTBL struct __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponseVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_get_Current(This,current)	\
    ( (This)->lpVtbl -> get_Current(This,current) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_get_HasCurrent(This,hasCurrent)	\
    ( (This)->lpVtbl -> get_HasCurrent(This,hasCurrent) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_MoveNext(This,hasCurrent)	\
    ( (This)->lpVtbl -> MoveNext(This,hasCurrent) ) 

#define __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_GetMany(This,capacity,items,actual)	\
    ( (This)->lpVtbl -> GetMany(This,capacity,items,actual) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_MidiServicePingResponseSummary_0000_0002 */
/* [local] */ 

#endif /* pinterface */
#endif /* DEF___FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse */


extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary_0000_0002_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary_0000_0002_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiServicePingResponseSummary2Eidl_0000_0199 */




extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary2Eidl_0000_0199_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary2Eidl_0000_0199_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiServicePingResponseSummary_0000_0003 */
/* [local] */ 

#ifndef DEF___FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse
#define DEF___FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse
#if !defined(__cplusplus) || defined(RO_NO_TEMPLATE_NAME)


extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary_0000_0003_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary_0000_0003_v0_0_s_ifspec;

#ifndef ____FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_INTERFACE_DEFINED__
#define ____FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_INTERFACE_DEFINED__

/* interface __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID___FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("91bdd7e6-b904-5a44-9f0b-31fd0375481f")
    __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse : public IInspectable
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE First( 
            /* [retval][out] */ __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse **first) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponseVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse, First)
        HRESULT ( STDMETHODCALLTYPE *First )( 
            __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [retval][out] */ __FIIterator_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse **first);
        
        END_INTERFACE
    } __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponseVtbl;

    interface __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse
    {
        CONST_VTBL struct __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponseVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_First(This,first)	\
    ( (This)->lpVtbl -> First(This,first) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_MidiServicePingResponseSummary_0000_0004 */
/* [local] */ 

#endif /* pinterface */
#endif /* DEF___FIIterable_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse */


extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary_0000_0004_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary_0000_0004_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiServicePingResponseSummary2Eidl_0000_0200 */




extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary2Eidl_0000_0200_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary2Eidl_0000_0200_v0_0_s_ifspec;

/* interface __MIDL_itf_MidiServicePingResponseSummary_0000_0005 */
/* [local] */ 

#ifndef DEF___FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse
#define DEF___FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse
#if !defined(__cplusplus) || defined(RO_NO_TEMPLATE_NAME)


extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary_0000_0005_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary_0000_0005_v0_0_s_ifspec;

#ifndef ____FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_INTERFACE_DEFINED__
#define ____FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_INTERFACE_DEFINED__

/* interface __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID___FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("7f95691b-f6e1-5fde-8f97-a79470005685")
    __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse : public IInspectable
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetAt( 
            /* [in] */ unsigned int index,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponse **item) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Size( 
            /* [retval][out] */ unsigned int *size) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE IndexOf( 
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponse *item,
            /* [out] */ unsigned int *index,
            /* [retval][out] */ boolean *found) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMany( 
            /* [in] */ unsigned int startIndex,
            /* [in] */ unsigned int capacity,
            /* [size_is][length_is][out] */ __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponse **items,
            /* [retval][out] */ unsigned int *actual) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponseVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse, GetAt)
        HRESULT ( STDMETHODCALLTYPE *GetAt )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [in] */ unsigned int index,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponse **item);
        
        DECLSPEC_XFGVIRT(__FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse, get_Size)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Size )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [retval][out] */ unsigned int *size);
        
        DECLSPEC_XFGVIRT(__FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse, IndexOf)
        HRESULT ( STDMETHODCALLTYPE *IndexOf )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponse *item,
            /* [out] */ unsigned int *index,
            /* [retval][out] */ boolean *found);
        
        DECLSPEC_XFGVIRT(__FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse, GetMany)
        HRESULT ( STDMETHODCALLTYPE *GetMany )( 
            __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse * This,
            /* [in] */ unsigned int startIndex,
            /* [in] */ unsigned int capacity,
            /* [size_is][length_is][out] */ __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponse **items,
            /* [retval][out] */ unsigned int *actual);
        
        END_INTERFACE
    } __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponseVtbl;

    interface __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse
    {
        CONST_VTBL struct __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponseVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_GetAt(This,index,item)	\
    ( (This)->lpVtbl -> GetAt(This,index,item) ) 

#define __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_get_Size(This,size)	\
    ( (This)->lpVtbl -> get_Size(This,size) ) 

#define __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_IndexOf(This,item,index,found)	\
    ( (This)->lpVtbl -> IndexOf(This,item,index,found) ) 

#define __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_GetMany(This,startIndex,capacity,items,actual)	\
    ( (This)->lpVtbl -> GetMany(This,startIndex,capacity,items,actual) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_MidiServicePingResponseSummary_0000_0006 */
/* [local] */ 

#endif /* pinterface */
#endif /* DEF___FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse */


extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary_0000_0006_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiServicePingResponseSummary_0000_0006_v0_0_s_ifspec;

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-dd0030003000")
    __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary : public IInspectable
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Success( 
            /* [retval][out] */ boolean *value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_FailureReason( 
            /* [retval][out] */ HSTRING *value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_TotalPingRoundTripMidiClock( 
            /* [retval][out] */ unsigned __int64 *value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_AveragePingRoundTripMidiClock( 
            /* [retval][out] */ unsigned __int64 *value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Responses( 
            /* [retval][out] */ __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse **value) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummaryVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary, get_Success)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Success )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary * This,
            /* [retval][out] */ boolean *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary, get_FailureReason)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_FailureReason )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary * This,
            /* [retval][out] */ HSTRING *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary, get_TotalPingRoundTripMidiClock)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_TotalPingRoundTripMidiClock )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary * This,
            /* [retval][out] */ unsigned __int64 *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary, get_AveragePingRoundTripMidiClock)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_AveragePingRoundTripMidiClock )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary * This,
            /* [retval][out] */ unsigned __int64 *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary, get_Responses)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Responses )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary * This,
            /* [retval][out] */ __FIVectorView_1_Windows__CDevices__CMidi2__CDiagnostics__CMidiServicePingResponse **value);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummaryVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummaryVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary_get_Success(This,value)	\
    ( (This)->lpVtbl -> get_Success(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary_get_FailureReason(This,value)	\
    ( (This)->lpVtbl -> get_FailureReason(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary_get_TotalPingRoundTripMidiClock(This,value)	\
    ( (This)->lpVtbl -> get_TotalPingRoundTripMidiClock(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary_get_AveragePingRoundTripMidiClock(This,value)	\
    ( (This)->lpVtbl -> get_AveragePingRoundTripMidiClock(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary_get_Responses(This,value)	\
    ( (This)->lpVtbl -> get_Responses(This,value) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary_INTERFACE_DEFINED__ */


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


