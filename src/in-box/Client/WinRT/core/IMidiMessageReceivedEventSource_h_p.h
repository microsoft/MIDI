

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for C:\Users\Pete\AppData\Local\Temp\IMidiMessageReceivedEventSource.idl-e9f0f72d:
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

#ifndef __IMidiMessageReceivedEventSource_h_p_h__
#define __IMidiMessageReceivedEventSource_h_p_h__

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

#ifndef ____FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs_FWD_DEFINED__
#define ____FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs_FWD_DEFINED__
typedef interface __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs;

#endif 	/* ____FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs_FWD_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource_FWD_DEFINED__ */


/* header files for imported files */
#include "Windows.Foundation.h"
#include "MidiApiContracts.h"
#include "MidiMessageReceivedEventArgs.h"
#include "IMidiEndpointConnectionSource.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_IMidiMessageReceivedEventSource_0000_0000 */
/* [local] */ 






#pragma once 


extern RPC_IF_HANDLE __MIDL_itf_IMidiMessageReceivedEventSource_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_IMidiMessageReceivedEventSource_0000_0000_v0_0_s_ifspec;

/* interface __MIDL_itf_IMidiMessageReceivedEventSource2Eidl_0000_0195 */




extern RPC_IF_HANDLE __MIDL_itf_IMidiMessageReceivedEventSource2Eidl_0000_0195_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_IMidiMessageReceivedEventSource2Eidl_0000_0195_v0_0_s_ifspec;

/* interface __MIDL_itf_IMidiMessageReceivedEventSource_0000_0001 */
/* [local] */ 

#ifndef DEF___FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs
#define DEF___FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs
#if !defined(__cplusplus) || defined(RO_NO_TEMPLATE_NAME)


extern RPC_IF_HANDLE __MIDL_itf_IMidiMessageReceivedEventSource_0000_0001_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_IMidiMessageReceivedEventSource_0000_0001_v0_0_s_ifspec;

#ifndef ____FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs_INTERFACE_DEFINED__
#define ____FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs_INTERFACE_DEFINED__

/* interface __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID___FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("1e455f72-676a-5d37-bd8d-fb1f2c02efe1")
    __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Invoke( 
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource *sender,
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs *e) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgsVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs * This);
        
        DECLSPEC_XFGVIRT(__FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs, Invoke)
        HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs * This,
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource *sender,
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventArgs *e);
        
        END_INTERFACE
    } __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgsVtbl;

    interface __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs
    {
        CONST_VTBL struct __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs_Invoke(This,sender,e)	\
    ( (This)->lpVtbl -> Invoke(This,sender,e) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_IMidiMessageReceivedEventSource_0000_0002 */
/* [local] */ 

#endif /* pinterface */
#endif /* DEF___FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs */


extern RPC_IF_HANDLE __MIDL_itf_IMidiMessageReceivedEventSource_0000_0002_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_IMidiMessageReceivedEventSource_0000_0002_v0_0_s_ifspec;

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource */
/* [uuid][object] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-ff001000F050")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource : public IInspectable
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE add_MessageReceived( 
            /* [in] */ __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs *handler,
            /* [out][retval] */ EventRegistrationToken *token) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE remove_MessageReceived( 
            /* [in] */ EventRegistrationToken token) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetEndpointConnectionSource( 
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource **result) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSourceVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource, add_MessageReceived)
        HRESULT ( STDMETHODCALLTYPE *add_MessageReceived )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource * This,
            /* [in] */ __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiMessageReceivedEventSource_Windows__CDevices__CMidi2__CMidiMessageReceivedEventArgs *handler,
            /* [out][retval] */ EventRegistrationToken *token);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource, remove_MessageReceived)
        HRESULT ( STDMETHODCALLTYPE *remove_MessageReceived )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource * This,
            /* [in] */ EventRegistrationToken token);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource, GetEndpointConnectionSource)
        HRESULT ( STDMETHODCALLTYPE *GetEndpointConnectionSource )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource * This,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource **result);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSourceVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSourceVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource_add_MessageReceived(This,handler,token)	\
    ( (This)->lpVtbl -> add_MessageReceived(This,handler,token) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource_remove_MessageReceived(This,token)	\
    ( (This)->lpVtbl -> remove_MessageReceived(This,token) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource_GetEndpointConnectionSource(This,result)	\
    ( (This)->lpVtbl -> GetEndpointConnectionSource(This,result) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessageReceivedEventSource_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


