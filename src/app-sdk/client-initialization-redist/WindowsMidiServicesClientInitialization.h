

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for WindowsMidiServicesClientInitialization.idl:
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

#ifndef __WindowsMidiServicesClientInitialization_h__
#define __WindowsMidiServicesClientInitialization_h__

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

/* Forward Declarations */ 

#ifndef __IMidiClientInitializer_FWD_DEFINED__
#define __IMidiClientInitializer_FWD_DEFINED__
typedef interface IMidiClientInitializer IMidiClientInitializer;

#endif 	/* __IMidiClientInitializer_FWD_DEFINED__ */


#ifndef __MidiClientInitializer_FWD_DEFINED__
#define __MidiClientInitializer_FWD_DEFINED__

#ifdef __cplusplus
typedef class MidiClientInitializer MidiClientInitializer;
#else
typedef struct MidiClientInitializer MidiClientInitializer;
#endif /* __cplusplus */

#endif 	/* __MidiClientInitializer_FWD_DEFINED__ */


/* header files for imported files */
#include "unknwn.h"
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_WindowsMidiServicesClientInitialization_0000_0000 */
/* [local] */ 

typedef /* [public][public] */ 
enum __MIDL___MIDL_itf_WindowsMidiServicesClientInitialization_0000_0000_0001
    {
        Platform_x64	= 1,
        Platform_Arm64X	= 4
    } 	MidiAppSDKPlatform;



extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServicesClientInitialization_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServicesClientInitialization_0000_0000_v0_0_s_ifspec;

#ifndef __IMidiClientInitializer_INTERFACE_DEFINED__
#define __IMidiClientInitializer_INTERFACE_DEFINED__

/* interface IMidiClientInitializer */
/* [helpstring][uuid][local][object] */ 


EXTERN_C const IID IID_IMidiClientInitializer;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-d551-bce2-1ead-a2500d50c580")
    IMidiClientInitializer : public IUnknown
    {
    public:
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE Initialize( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE GetInstalledWindowsMidiServicesSdkVersion( 
            /* [optional][out] */ MidiAppSDKPlatform *buildPlatform,
            /* [optional][out] */ DWORD *versionMajor,
            /* [optional][out] */ DWORD *versionMinor,
            /* [optional][out] */ DWORD *versionRevision,
            /* [optional][out] */ DWORD *versionDateNumber,
            /* [optional][out] */ DWORD *versionTimeNumber,
            /* [optional][string][out] */ LPWSTR *buildSource,
            /* [optional][string][out] */ LPWSTR *versionName,
            /* [optional][string][out] */ LPWSTR *versionFullString) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE EnsureServiceAvailable( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE Shutdown( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiClientInitializerVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiClientInitializer * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiClientInitializer * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiClientInitializer * This);
        
        DECLSPEC_XFGVIRT(IMidiClientInitializer, Initialize)
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *Initialize )( 
            IMidiClientInitializer * This);
        
        DECLSPEC_XFGVIRT(IMidiClientInitializer, GetInstalledWindowsMidiServicesSdkVersion)
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *GetInstalledWindowsMidiServicesSdkVersion )( 
            IMidiClientInitializer * This,
            /* [optional][out] */ MidiAppSDKPlatform *buildPlatform,
            /* [optional][out] */ DWORD *versionMajor,
            /* [optional][out] */ DWORD *versionMinor,
            /* [optional][out] */ DWORD *versionRevision,
            /* [optional][out] */ DWORD *versionDateNumber,
            /* [optional][out] */ DWORD *versionTimeNumber,
            /* [optional][string][out] */ LPWSTR *buildSource,
            /* [optional][string][out] */ LPWSTR *versionName,
            /* [optional][string][out] */ LPWSTR *versionFullString);
        
        DECLSPEC_XFGVIRT(IMidiClientInitializer, EnsureServiceAvailable)
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *EnsureServiceAvailable )( 
            IMidiClientInitializer * This);
        
        DECLSPEC_XFGVIRT(IMidiClientInitializer, Shutdown)
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *Shutdown )( 
            IMidiClientInitializer * This);
        
        END_INTERFACE
    } IMidiClientInitializerVtbl;

    interface IMidiClientInitializer
    {
        CONST_VTBL struct IMidiClientInitializerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiClientInitializer_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiClientInitializer_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiClientInitializer_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiClientInitializer_Initialize(This)	\
    ( (This)->lpVtbl -> Initialize(This) ) 

#define IMidiClientInitializer_GetInstalledWindowsMidiServicesSdkVersion(This,buildPlatform,versionMajor,versionMinor,versionRevision,versionDateNumber,versionTimeNumber,buildSource,versionName,versionFullString)	\
    ( (This)->lpVtbl -> GetInstalledWindowsMidiServicesSdkVersion(This,buildPlatform,versionMajor,versionMinor,versionRevision,versionDateNumber,versionTimeNumber,buildSource,versionName,versionFullString) ) 

#define IMidiClientInitializer_EnsureServiceAvailable(This)	\
    ( (This)->lpVtbl -> EnsureServiceAvailable(This) ) 

#define IMidiClientInitializer_Shutdown(This)	\
    ( (This)->lpVtbl -> Shutdown(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiClientInitializer_INTERFACE_DEFINED__ */



#ifndef __WindowsMidiServicesClientInitializationLib_LIBRARY_DEFINED__
#define __WindowsMidiServicesClientInitializationLib_LIBRARY_DEFINED__

/* library WindowsMidiServicesClientInitializationLib */
/* [helpstring][version][uuid] */ 


EXTERN_C const IID LIBID_WindowsMidiServicesClientInitializationLib;

EXTERN_C const CLSID CLSID_MidiClientInitializer;

#ifdef __cplusplus

class DECLSPEC_UUID("c3263827-c3b0-bdbd-2500-ce63a3f3f2c3")
MidiClientInitializer;
#endif
#endif /* __WindowsMidiServicesClientInitializationLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


