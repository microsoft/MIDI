

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for C:\Users\Pete\AppData\Local\Temp\MidiDiagnostics.idl-db8390d9:
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

#ifndef __MidiDiagnostics_h_p_h__
#define __MidiDiagnostics_h_p_h__

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

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics_FWD_DEFINED__ */


/* header files for imported files */
#include "MidiApiContracts.h"
#include "Windows.Foundation.h"
#include "MidiServicePingResponseSummary.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_MidiDiagnostics_0000_0000 */
/* [local] */ 



#pragma once 


extern RPC_IF_HANDLE __MIDL_itf_MidiDiagnostics_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiDiagnostics_0000_0000_v0_0_s_ifspec;

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-ee0030001000")
    __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics : public IInspectable
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_DiagnosticsLoopbackAEndpointDeviceId( 
            /* [retval][out] */ HSTRING *value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_DiagnosticsLoopbackBEndpointDeviceId( 
            /* [retval][out] */ HSTRING *value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE PingService( 
            /* [in] */ byte pingCount,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary **result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE PingService2( 
            /* [in] */ byte pingCount,
            /* [in] */ unsigned int timeoutMilliseconds,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary **result) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStaticsVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics, get_DiagnosticsLoopbackAEndpointDeviceId)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_DiagnosticsLoopbackAEndpointDeviceId )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics * This,
            /* [retval][out] */ HSTRING *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics, get_DiagnosticsLoopbackBEndpointDeviceId)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_DiagnosticsLoopbackBEndpointDeviceId )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics * This,
            /* [retval][out] */ HSTRING *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics, PingService)
        HRESULT ( STDMETHODCALLTYPE *PingService )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics * This,
            /* [in] */ byte pingCount,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary **result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics, PingService2)
        HRESULT ( STDMETHODCALLTYPE *PingService2 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics * This,
            /* [in] */ byte pingCount,
            /* [in] */ unsigned int timeoutMilliseconds,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiServicePingResponseSummary **result);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStaticsVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStaticsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics_get_DiagnosticsLoopbackAEndpointDeviceId(This,value)	\
    ( (This)->lpVtbl -> get_DiagnosticsLoopbackAEndpointDeviceId(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics_get_DiagnosticsLoopbackBEndpointDeviceId(This,value)	\
    ( (This)->lpVtbl -> get_DiagnosticsLoopbackBEndpointDeviceId(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics_PingService(This,pingCount,result)	\
    ( (This)->lpVtbl -> PingService(This,pingCount,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics_PingService2(This,pingCount,timeoutMilliseconds,result)	\
    ( (This)->lpVtbl -> PingService2(This,pingCount,timeoutMilliseconds,result) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CDiagnostics_CIMidiDiagnosticsStatics_INTERFACE_DEFINED__ */


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


