

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for C:\Users\Pete\AppData\Local\Temp\MidiEndpointConnectionBasicSettings.idl-104657e7:
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

#ifndef __MidiEndpointConnectionBasicSettings_h_p_h__
#define __MidiEndpointConnectionBasicSettings_h_p_h__

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

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings_FWD_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory_FWD_DEFINED__ */


/* header files for imported files */
#include "Windows.Foundation.h"
#include "MidiApiContracts.h"
#include "IMidiEndpointConnectionSettings.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_MidiEndpointConnectionBasicSettings_0000_0000 */
/* [local] */ 




#pragma once 



extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnectionBasicSettings_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiEndpointConnectionBasicSettings_0000_0000_v0_0_s_ifspec;

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-dd0010006000")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings : public IInspectable
    {
    public:
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings * This,
            /* [out] */ TrustLevel *trustLevel);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings_INTERFACE_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("3ba8921a-f702-5430-89c9-d31a6ca46c56")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory : public IInspectable
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE CreateInstance( 
            /* [in] */ boolean waitForEndpointReceiptOnSend,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings **value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE CreateInstance2( 
            /* [in] */ boolean waitForEndpointReceiptOnSend,
            /* [in] */ boolean autoReconnect,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings **value) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactoryVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory, CreateInstance)
        HRESULT ( STDMETHODCALLTYPE *CreateInstance )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory * This,
            /* [in] */ boolean waitForEndpointReceiptOnSend,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings **value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory, CreateInstance2)
        HRESULT ( STDMETHODCALLTYPE *CreateInstance2 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory * This,
            /* [in] */ boolean waitForEndpointReceiptOnSend,
            /* [in] */ boolean autoReconnect,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettings **value);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactoryVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactoryVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory_CreateInstance(This,waitForEndpointReceiptOnSend,value)	\
    ( (This)->lpVtbl -> CreateInstance(This,waitForEndpointReceiptOnSend,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory_CreateInstance2(This,waitForEndpointReceiptOnSend,autoReconnect,value)	\
    ( (This)->lpVtbl -> CreateInstance2(This,waitForEndpointReceiptOnSend,autoReconnect,value) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionBasicSettingsFactory_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


