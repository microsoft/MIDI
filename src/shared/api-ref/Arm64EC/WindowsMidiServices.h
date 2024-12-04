

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for WindowsMidiServices.idl:
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

#ifndef __WindowsMidiServices_h__
#define __WindowsMidiServices_h__

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

#ifndef __IMidiTransport_FWD_DEFINED__
#define __IMidiTransport_FWD_DEFINED__
typedef interface IMidiTransport IMidiTransport;

#endif 	/* __IMidiTransport_FWD_DEFINED__ */


#ifndef __IMidiCallback_FWD_DEFINED__
#define __IMidiCallback_FWD_DEFINED__
typedef interface IMidiCallback IMidiCallback;

#endif 	/* __IMidiCallback_FWD_DEFINED__ */


#ifndef __IMidiIn_FWD_DEFINED__
#define __IMidiIn_FWD_DEFINED__
typedef interface IMidiIn IMidiIn;

#endif 	/* __IMidiIn_FWD_DEFINED__ */


#ifndef __IMidiOut_FWD_DEFINED__
#define __IMidiOut_FWD_DEFINED__
typedef interface IMidiOut IMidiOut;

#endif 	/* __IMidiOut_FWD_DEFINED__ */


#ifndef __IMidiBiDi_FWD_DEFINED__
#define __IMidiBiDi_FWD_DEFINED__
typedef interface IMidiBiDi IMidiBiDi;

#endif 	/* __IMidiBiDi_FWD_DEFINED__ */


#ifndef __IMidiDeviceManagerInterface_FWD_DEFINED__
#define __IMidiDeviceManagerInterface_FWD_DEFINED__
typedef interface IMidiDeviceManagerInterface IMidiDeviceManagerInterface;

#endif 	/* __IMidiDeviceManagerInterface_FWD_DEFINED__ */


#ifndef __IMidiServiceConfigurationManagerInterface_FWD_DEFINED__
#define __IMidiServiceConfigurationManagerInterface_FWD_DEFINED__
typedef interface IMidiServiceConfigurationManagerInterface IMidiServiceConfigurationManagerInterface;

#endif 	/* __IMidiServiceConfigurationManagerInterface_FWD_DEFINED__ */


#ifndef __IMidiTransportConfigurationManager_FWD_DEFINED__
#define __IMidiTransportConfigurationManager_FWD_DEFINED__
typedef interface IMidiTransportConfigurationManager IMidiTransportConfigurationManager;

#endif 	/* __IMidiTransportConfigurationManager_FWD_DEFINED__ */


#ifndef __IMidiTransform_FWD_DEFINED__
#define __IMidiTransform_FWD_DEFINED__
typedef interface IMidiTransform IMidiTransform;

#endif 	/* __IMidiTransform_FWD_DEFINED__ */


#ifndef __IMidiDataTransform_FWD_DEFINED__
#define __IMidiDataTransform_FWD_DEFINED__
typedef interface IMidiDataTransform IMidiDataTransform;

#endif 	/* __IMidiDataTransform_FWD_DEFINED__ */


#ifndef __IMidiProtocolNegotiationCompleteCallback_FWD_DEFINED__
#define __IMidiProtocolNegotiationCompleteCallback_FWD_DEFINED__
typedef interface IMidiProtocolNegotiationCompleteCallback IMidiProtocolNegotiationCompleteCallback;

#endif 	/* __IMidiProtocolNegotiationCompleteCallback_FWD_DEFINED__ */


#ifndef __IMidiEndpointProtocolManagerInterface_FWD_DEFINED__
#define __IMidiEndpointProtocolManagerInterface_FWD_DEFINED__
typedef interface IMidiEndpointProtocolManagerInterface IMidiEndpointProtocolManagerInterface;

#endif 	/* __IMidiEndpointProtocolManagerInterface_FWD_DEFINED__ */


#ifndef __IMidiEndpointManager_FWD_DEFINED__
#define __IMidiEndpointManager_FWD_DEFINED__
typedef interface IMidiEndpointManager IMidiEndpointManager;

#endif 	/* __IMidiEndpointManager_FWD_DEFINED__ */


#ifndef __IMidiServiceTransportPluginMetadataProvider_FWD_DEFINED__
#define __IMidiServiceTransportPluginMetadataProvider_FWD_DEFINED__
typedef interface IMidiServiceTransportPluginMetadataProvider IMidiServiceTransportPluginMetadataProvider;

#endif 	/* __IMidiServiceTransportPluginMetadataProvider_FWD_DEFINED__ */


#ifndef __IMidiServiceTransformPluginMetadataProvider_FWD_DEFINED__
#define __IMidiServiceTransformPluginMetadataProvider_FWD_DEFINED__
typedef interface IMidiServiceTransformPluginMetadataProvider IMidiServiceTransformPluginMetadataProvider;

#endif 	/* __IMidiServiceTransformPluginMetadataProvider_FWD_DEFINED__ */


#ifndef __IMidiServicePluginMetadataReporterInterface_FWD_DEFINED__
#define __IMidiServicePluginMetadataReporterInterface_FWD_DEFINED__
typedef interface IMidiServicePluginMetadataReporterInterface IMidiServicePluginMetadataReporterInterface;

#endif 	/* __IMidiServicePluginMetadataReporterInterface_FWD_DEFINED__ */


#ifndef __IMidiSessionTracker_FWD_DEFINED__
#define __IMidiSessionTracker_FWD_DEFINED__
typedef interface IMidiSessionTracker IMidiSessionTracker;

#endif 	/* __IMidiSessionTracker_FWD_DEFINED__ */


/* header files for imported files */
#include "unknwn.h"
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_WindowsMidiServices_0000_0000 */
/* [local] */ 

#ifdef MIDL_PASS
#pragma once
#pragma warning(push)
#pragma warning(disable:4668) 
#pragma once
#pragma region Input Buffer SAL 1 compatibility macros
#pragma endregion Input Buffer SAL 1 compatibility macros
#pragma once
#pragma once
#pragma warning(pop)
#pragma once
typedef 
enum _SW_DEVICE_CAPABILITIES
    {
        SWDeviceCapabilitiesNone	= 0,
        SWDeviceCapabilitiesRemovable	= 0x1,
        SWDeviceCapabilitiesSilentInstall	= 0x2,
        SWDeviceCapabilitiesNoDisplayInUI	= 0x4,
        SWDeviceCapabilitiesDriverRequired	= 0x8
    } 	SW_DEVICE_CAPABILITIES;

typedef enum _SW_DEVICE_CAPABILITIES *PSW_DEVICE_CAPABILITIES;

typedef struct _SW_DEVICE_CREATE_INFO
    {
    ULONG cbSize;
    LPCWSTR pszInstanceId;
    LPCWSTR pszzHardwareIds;
    LPCWSTR pszzCompatibleIds;
    const GUID *pContainerId;
    ULONG CapabilityFlags;
    LPCWSTR pszDeviceDescription;
    LPCWSTR pszDeviceLocation;
    const SECURITY_DESCRIPTOR *pSecurityDescriptor;
    } 	SW_DEVICE_CREATE_INFO;

typedef struct _SW_DEVICE_CREATE_INFO *PSW_DEVICE_CREATE_INFO;

typedef 
enum _SW_DEVICE_LIFETIME
    {
        SWDeviceLifetimeHandle	= 0,
        SWDeviceLifetimeParentPresent	= ( SWDeviceLifetimeHandle + 1 ) ,
        SWDeviceLifetimeMax	= ( SWDeviceLifetimeParentPresent + 1 ) 
    } 	SW_DEVICE_LIFETIME;

typedef enum _SW_DEVICE_LIFETIME *PSW_DEVICE_LIFETIME;

#pragma warning(push)
#pragma warning(disable:4001) 
#pragma once
#pragma warning(push)
#pragma warning(disable:4001) 
#pragma once
#pragma warning(pop)
#pragma warning(pop)
#pragma region Desktop Family or OneCore Family or Games Family
typedef ULONG DEVPROPTYPE;

typedef ULONG *PDEVPROPTYPE;

typedef CHAR DEVPROP_BOOLEAN;

typedef CHAR *PDEVPROP_BOOLEAN;

typedef GUID DEVPROPGUID;

typedef GUID *PDEVPROPGUID;

typedef ULONG DEVPROPID;

typedef ULONG *PDEVPROPID;

typedef struct _DEVPROPKEY
    {
    DEVPROPGUID fmtid;
    DEVPROPID pid;
    } 	DEVPROPKEY;

typedef struct _DEVPROPKEY *PDEVPROPKEY;

typedef 
enum _DEVPROPSTORE
    {
        DEVPROP_STORE_SYSTEM	= 0,
        DEVPROP_STORE_USER	= ( DEVPROP_STORE_SYSTEM + 1 ) 
    } 	DEVPROPSTORE;

typedef enum _DEVPROPSTORE *PDEVPROPSTORE;

typedef struct _DEVPROPCOMPKEY
    {
    DEVPROPKEY Key;
    DEVPROPSTORE Store;
    LPCWSTR LocaleName;
    } 	DEVPROPCOMPKEY;

typedef struct _DEVPROPCOMPKEY *PDEVPROPCOMPKEY;

typedef struct _DEVPROPERTY
    {
    DEVPROPCOMPKEY CompKey;
    DEVPROPTYPE Type;
    ULONG BufferSize;
    PVOID Buffer;
    } 	DEVPROPERTY;

typedef struct _DEVPROPERTY *PDEVPROPERTY;

#pragma endregion
#else
#include <swdevicedef.h>
#include <devpropdef.h>
#endif
typedef /* [public][public][public][public][public][public][public][public][public][public][public][public][public][public][public][public] */ 
enum __MIDL___MIDL_itf_WindowsMidiServices_0000_0000_0001
    {
        MidiDataFormats_Invalid	= 0,
        MidiDataFormats_ByteStream	= 0x1,
        MidiDataFormats_UMP	= 0x2,
        MidiDataFormats_Any	= 0xffff
    } 	MidiDataFormats;

typedef /* [public][public] */ 
enum __MIDL___MIDL_itf_WindowsMidiServices_0000_0000_0002
    {
        MidiFlowIn	= 0,
        MidiFlowOut	= ( MidiFlowIn + 1 ) ,
        MidiFlowBidirectional	= ( MidiFlowOut + 1 ) 
    } 	MidiFlow;

typedef /* [public][public][public][public][public][public][public] */ 
enum __MIDL___MIDL_itf_WindowsMidiServices_0000_0000_0003
    {
        MetadataFlags_None	= 0,
        MetadataFlags_IsRuntimeCreatableByApps	= 1,
        MetadataFlags_IsRuntimeCreatableBySettings	= 2,
        MetadataFlags_IsSystemManaged	= 4,
        MetadataFlags_IsClientConfigurable	= 8
    } 	MetadataFlags;

typedef /* [public] */ struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0000_0004
    {
    MidiDataFormats DataFormat;
    } 	TRANSPORTCREATIONPARAMS;

typedef struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0000_0004 *PTRANSPORTCREATIONPARAMS;



extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0000_v0_0_s_ifspec;

#ifndef __IMidiTransport_INTERFACE_DEFINED__
#define __IMidiTransport_INTERFACE_DEFINED__

/* interface IMidiTransport */
/* [uuid][local][object] */ 


EXTERN_C const IID IID_IMidiTransport;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("EA264200-3328-49E5-8815-73649A8748BE")
    IMidiTransport : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Activate( 
            /* [annotation][in] */ 
            _In_  REFIID iid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **activatedInterface) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiTransportVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiTransport * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiTransport * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiTransport * This);
        
        DECLSPEC_XFGVIRT(IMidiTransport, Activate)
        HRESULT ( STDMETHODCALLTYPE *Activate )( 
            IMidiTransport * This,
            /* [annotation][in] */ 
            _In_  REFIID iid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **activatedInterface);
        
        END_INTERFACE
    } IMidiTransportVtbl;

    interface IMidiTransport
    {
        CONST_VTBL struct IMidiTransportVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiTransport_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiTransport_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiTransport_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiTransport_Activate(This,iid,activatedInterface)	\
    ( (This)->lpVtbl -> Activate(This,iid,activatedInterface) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiTransport_INTERFACE_DEFINED__ */


#ifndef __IMidiCallback_INTERFACE_DEFINED__
#define __IMidiCallback_INTERFACE_DEFINED__

/* interface IMidiCallback */
/* [uuid][local][object] */ 


EXTERN_C const IID IID_IMidiCallback;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("4D6A29E5-DF4F-4A2D-A923-9B23B3F2D6F6")
    IMidiCallback : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Callback( 
            /* [annotation][in] */ 
            _In_  PVOID message,
            /* [annotation][in] */ 
            _In_  UINT size,
            /* [annotation][in] */ 
            _In_  LONGLONG position,
            /* [annotation][in] */ 
            _In_  LONGLONG context) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiCallbackVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiCallback * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiCallback * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiCallback * This);
        
        DECLSPEC_XFGVIRT(IMidiCallback, Callback)
        HRESULT ( STDMETHODCALLTYPE *Callback )( 
            IMidiCallback * This,
            /* [annotation][in] */ 
            _In_  PVOID message,
            /* [annotation][in] */ 
            _In_  UINT size,
            /* [annotation][in] */ 
            _In_  LONGLONG position,
            /* [annotation][in] */ 
            _In_  LONGLONG context);
        
        END_INTERFACE
    } IMidiCallbackVtbl;

    interface IMidiCallback
    {
        CONST_VTBL struct IMidiCallbackVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiCallback_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiCallback_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiCallback_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiCallback_Callback(This,message,size,position,context)	\
    ( (This)->lpVtbl -> Callback(This,message,size,position,context) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiCallback_INTERFACE_DEFINED__ */


#ifndef __IMidiIn_INTERFACE_DEFINED__
#define __IMidiIn_INTERFACE_DEFINED__

/* interface IMidiIn */
/* [uuid][local][object] */ 


EXTERN_C const IID IID_IMidiIn;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("6C4B8BF9-8133-4B41-8303-1FDE78E20ACA")
    IMidiIn : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( 
            /* [annotation][string][in] */ 
            _In_  LPCWSTR endpointDeviceInterfaceId,
            /* [annotation][in] */ 
            _In_  PTRANSPORTCREATIONPARAMS creationParams,
            /* [annotation][out][in] */ 
            _Inout_  DWORD *mmcssTaskId,
            /* [annotation][in] */ 
            _In_  IMidiCallback *callback,
            /* [annotation][in] */ 
            _In_  LONGLONG context,
            /* [annotation][in] */ 
            _In_  GUID sessionId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Shutdown( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiInVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiIn * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiIn * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiIn * This);
        
        DECLSPEC_XFGVIRT(IMidiIn, Initialize)
        HRESULT ( STDMETHODCALLTYPE *Initialize )( 
            IMidiIn * This,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR endpointDeviceInterfaceId,
            /* [annotation][in] */ 
            _In_  PTRANSPORTCREATIONPARAMS creationParams,
            /* [annotation][out][in] */ 
            _Inout_  DWORD *mmcssTaskId,
            /* [annotation][in] */ 
            _In_  IMidiCallback *callback,
            /* [annotation][in] */ 
            _In_  LONGLONG context,
            /* [annotation][in] */ 
            _In_  GUID sessionId);
        
        DECLSPEC_XFGVIRT(IMidiIn, Shutdown)
        HRESULT ( STDMETHODCALLTYPE *Shutdown )( 
            IMidiIn * This);
        
        END_INTERFACE
    } IMidiInVtbl;

    interface IMidiIn
    {
        CONST_VTBL struct IMidiInVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiIn_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiIn_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiIn_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiIn_Initialize(This,endpointDeviceInterfaceId,creationParams,mmcssTaskId,callback,context,sessionId)	\
    ( (This)->lpVtbl -> Initialize(This,endpointDeviceInterfaceId,creationParams,mmcssTaskId,callback,context,sessionId) ) 

#define IMidiIn_Shutdown(This)	\
    ( (This)->lpVtbl -> Shutdown(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiIn_INTERFACE_DEFINED__ */


#ifndef __IMidiOut_INTERFACE_DEFINED__
#define __IMidiOut_INTERFACE_DEFINED__

/* interface IMidiOut */
/* [uuid][local][object] */ 


EXTERN_C const IID IID_IMidiOut;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("F328D645-7D6D-4924-A7E3-9DEE245189F9")
    IMidiOut : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( 
            /* [annotation][string][in] */ 
            _In_  LPCWSTR endpointDeviceInterfaceId,
            /* [annotation][in] */ 
            _In_  PTRANSPORTCREATIONPARAMS creationParams,
            /* [annotation][in] */ 
            _In_  DWORD *mmcssTaskId,
            /* [annotation][in] */ 
            _In_  GUID sessionId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Shutdown( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendMidiMessage( 
            /* [annotation][in] */ 
            _In_  PVOID message,
            /* [annotation][in] */ 
            _In_  UINT size,
            /* [annotation][in] */ 
            _In_  LONGLONG position) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiOutVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiOut * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiOut * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiOut * This);
        
        DECLSPEC_XFGVIRT(IMidiOut, Initialize)
        HRESULT ( STDMETHODCALLTYPE *Initialize )( 
            IMidiOut * This,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR endpointDeviceInterfaceId,
            /* [annotation][in] */ 
            _In_  PTRANSPORTCREATIONPARAMS creationParams,
            /* [annotation][in] */ 
            _In_  DWORD *mmcssTaskId,
            /* [annotation][in] */ 
            _In_  GUID sessionId);
        
        DECLSPEC_XFGVIRT(IMidiOut, Shutdown)
        HRESULT ( STDMETHODCALLTYPE *Shutdown )( 
            IMidiOut * This);
        
        DECLSPEC_XFGVIRT(IMidiOut, SendMidiMessage)
        HRESULT ( STDMETHODCALLTYPE *SendMidiMessage )( 
            IMidiOut * This,
            /* [annotation][in] */ 
            _In_  PVOID message,
            /* [annotation][in] */ 
            _In_  UINT size,
            /* [annotation][in] */ 
            _In_  LONGLONG position);
        
        END_INTERFACE
    } IMidiOutVtbl;

    interface IMidiOut
    {
        CONST_VTBL struct IMidiOutVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiOut_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiOut_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiOut_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiOut_Initialize(This,endpointDeviceInterfaceId,creationParams,mmcssTaskId,sessionId)	\
    ( (This)->lpVtbl -> Initialize(This,endpointDeviceInterfaceId,creationParams,mmcssTaskId,sessionId) ) 

#define IMidiOut_Shutdown(This)	\
    ( (This)->lpVtbl -> Shutdown(This) ) 

#define IMidiOut_SendMidiMessage(This,message,size,position)	\
    ( (This)->lpVtbl -> SendMidiMessage(This,message,size,position) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiOut_INTERFACE_DEFINED__ */


#ifndef __IMidiBiDi_INTERFACE_DEFINED__
#define __IMidiBiDi_INTERFACE_DEFINED__

/* interface IMidiBiDi */
/* [uuid][local][object] */ 


EXTERN_C const IID IID_IMidiBiDi;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("B89BBB45-7001-4BEA-BBD8-C7CC26E7836C")
    IMidiBiDi : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( 
            /* [annotation][string][in] */ 
            _In_  LPCWSTR endpointDeviceInterfaceId,
            /* [annotation][in] */ 
            _In_  PTRANSPORTCREATIONPARAMS creationParams,
            /* [annotation][out][in] */ 
            _Inout_  DWORD *mmcssTaskId,
            /* [annotation][in] */ 
            _In_  IMidiCallback *callback,
            /* [annotation][in] */ 
            _In_  LONGLONG context,
            /* [annotation][in] */ 
            _In_  GUID sessionId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Shutdown( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendMidiMessage( 
            /* [annotation][in] */ 
            _In_  PVOID message,
            /* [annotation][in] */ 
            _In_  UINT size,
            /* [annotation][in] */ 
            _In_  LONGLONG position) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiBiDiVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiBiDi * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiBiDi * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiBiDi * This);
        
        DECLSPEC_XFGVIRT(IMidiBiDi, Initialize)
        HRESULT ( STDMETHODCALLTYPE *Initialize )( 
            IMidiBiDi * This,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR endpointDeviceInterfaceId,
            /* [annotation][in] */ 
            _In_  PTRANSPORTCREATIONPARAMS creationParams,
            /* [annotation][out][in] */ 
            _Inout_  DWORD *mmcssTaskId,
            /* [annotation][in] */ 
            _In_  IMidiCallback *callback,
            /* [annotation][in] */ 
            _In_  LONGLONG context,
            /* [annotation][in] */ 
            _In_  GUID sessionId);
        
        DECLSPEC_XFGVIRT(IMidiBiDi, Shutdown)
        HRESULT ( STDMETHODCALLTYPE *Shutdown )( 
            IMidiBiDi * This);
        
        DECLSPEC_XFGVIRT(IMidiBiDi, SendMidiMessage)
        HRESULT ( STDMETHODCALLTYPE *SendMidiMessage )( 
            IMidiBiDi * This,
            /* [annotation][in] */ 
            _In_  PVOID message,
            /* [annotation][in] */ 
            _In_  UINT size,
            /* [annotation][in] */ 
            _In_  LONGLONG position);
        
        END_INTERFACE
    } IMidiBiDiVtbl;

    interface IMidiBiDi
    {
        CONST_VTBL struct IMidiBiDiVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiBiDi_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiBiDi_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiBiDi_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiBiDi_Initialize(This,endpointDeviceInterfaceId,creationParams,mmcssTaskId,callback,context,sessionId)	\
    ( (This)->lpVtbl -> Initialize(This,endpointDeviceInterfaceId,creationParams,mmcssTaskId,callback,context,sessionId) ) 

#define IMidiBiDi_Shutdown(This)	\
    ( (This)->lpVtbl -> Shutdown(This) ) 

#define IMidiBiDi_SendMidiMessage(This,message,size,position)	\
    ( (This)->lpVtbl -> SendMidiMessage(This,message,size,position) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiBiDi_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_WindowsMidiServices_0000_0005 */
/* [local] */ 

typedef /* [public][public][public][public] */ 
enum __MIDL___MIDL_itf_WindowsMidiServices_0000_0005_0001
    {
        MidiEndpointDeviceType_Normal	= 0,
        MidiEndpointDeviceType_VirtualDeviceResponder	= 100,
        MidiEndpointDeviceType_MidiSynthesizer	= 400,
        MidiEndpointDeviceType_DiagnosticLoopback	= 500,
        MidiEndpointDeviceType_DiagnosticPing	= 510
    } 	MidiEndpointDeviceType;

typedef /* [public][public][public][public][public][public][public] */ 
enum __MIDL___MIDL_itf_WindowsMidiServices_0000_0005_0002
    {
        MidiEndpointCapabilities_None	= 0,
        MidiEndpointCapabilities_SupportsMidi1Protocol	= 1,
        MidiEndpointCapabilities_SupportsMidi2Protocol	= 2,
        MidiEndpointCapabilities_SupportsMultiClient	= 4,
        MidiEndpointCapabilities_GenerateIncomingTimestamps	= 8
    } 	MidiEndpointCapabilities;

typedef /* [public] */ struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0005_0003
    {
    GUID TransportId;
    MidiEndpointDeviceType EndpointDeviceType;
    LPCWSTR FriendlyName;
    LPCWSTR TransportCode;
    LPCWSTR EndpointName;
    LPCWSTR EndpointDescription;
    LPCWSTR CustomEndpointName;
    LPCWSTR CustomEndpointDescription;
    LPCWSTR UniqueIdentifier;
    LPCWSTR ManufacturerName;
    MidiDataFormats SupportedDataFormats;
    MidiDataFormats NativeDataFormat;
    MidiEndpointCapabilities Capabilities;
    } 	MIDIENDPOINTCOMMONPROPERTIES;

typedef struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0005_0003 *PMIDIENDPOINTCOMMONPROPERTIES;



extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0005_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0005_v0_0_s_ifspec;

#ifndef __IMidiDeviceManagerInterface_INTERFACE_DEFINED__
#define __IMidiDeviceManagerInterface_INTERFACE_DEFINED__

/* interface IMidiDeviceManagerInterface */
/* [uuid][local][object] */ 


EXTERN_C const IID IID_IMidiDeviceManagerInterface;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("A04374D3-4514-44E1-A2F9-7D8B907AEF1F")
    IMidiDeviceManagerInterface : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE ActivateVirtualParentDevice( 
            /* [annotation][in] */ 
            _In_  ULONG devPropertyCount,
            /* [annotation][size_is][in] */ 
            _In_opt_  const DEVPROPERTY *devProperties,
            /* [in] */ const SW_DEVICE_CREATE_INFO *createInfo,
            /* [annotation][string][out] */ 
            _Out_opt_  LPWSTR *createdDeviceId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ActivateEndpoint( 
            /* [annotation][string][in] */ 
            _In_  LPCWSTR parentInstanceId,
            /* [annotation][in] */ 
            _In_  BOOL umpOnly,
            /* [annotation][in] */ 
            _In_  MidiFlow midiFlow,
            /* [annotation][in] */ 
            _In_  PMIDIENDPOINTCOMMONPROPERTIES commonProperties,
            /* [annotation][in] */ 
            _In_  ULONG intPropertyCount,
            /* [annotation][in] */ 
            _In_  ULONG devPropertyCount,
            /* [annotation][size_is][in] */ 
            _In_opt_  const DEVPROPERTY *interfaceDevProperties,
            /* [annotation][size_is][in] */ 
            _In_opt_  const DEVPROPERTY *deviceDevProperties,
            /* [annotation][in] */ 
            _In_opt_  const SW_DEVICE_CREATE_INFO *createInfo,
            /* [annotation][string][out] */ 
            _Out_opt_  LPWSTR *createdEndpointDeviceInterfaceId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE UpdateEndpointProperties( 
            /* [annotation][string][in] */ 
            _In_  LPCWSTR endpointDeviceInterfaceId,
            /* [annotation][in] */ 
            _In_  ULONG intPropertyCount,
            /* [annotation][size_is][in] */ 
            _In_  const DEVPROPERTY *interfaceDevProperties) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE DeleteEndpointProperties( 
            /* [annotation][string][in] */ 
            _In_  LPCWSTR endpointDeviceInterfaceId,
            /* [annotation][in] */ 
            _In_  ULONG devPropertyCount,
            /* [annotation][size_is][in] */ 
            _In_opt_  const DEVPROPERTY *devPropKeys) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE DeactivateEndpoint( 
            /* [annotation][string][in] */ 
            _In_  LPCWSTR instanceId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE RemoveEndpoint( 
            /* [annotation][string][in] */ 
            _In_  LPCWSTR instanceId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE UpdateTransportConfiguration( 
            /* [annotation][in] */ 
            _In_  GUID transportId,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR configurationJson,
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *responseJson) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiDeviceManagerInterfaceVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiDeviceManagerInterface * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiDeviceManagerInterface * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiDeviceManagerInterface * This);
        
        DECLSPEC_XFGVIRT(IMidiDeviceManagerInterface, ActivateVirtualParentDevice)
        HRESULT ( STDMETHODCALLTYPE *ActivateVirtualParentDevice )( 
            IMidiDeviceManagerInterface * This,
            /* [annotation][in] */ 
            _In_  ULONG devPropertyCount,
            /* [annotation][size_is][in] */ 
            _In_opt_  const DEVPROPERTY *devProperties,
            /* [in] */ const SW_DEVICE_CREATE_INFO *createInfo,
            /* [annotation][string][out] */ 
            _Out_opt_  LPWSTR *createdDeviceId);
        
        DECLSPEC_XFGVIRT(IMidiDeviceManagerInterface, ActivateEndpoint)
        HRESULT ( STDMETHODCALLTYPE *ActivateEndpoint )( 
            IMidiDeviceManagerInterface * This,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR parentInstanceId,
            /* [annotation][in] */ 
            _In_  BOOL umpOnly,
            /* [annotation][in] */ 
            _In_  MidiFlow midiFlow,
            /* [annotation][in] */ 
            _In_  PMIDIENDPOINTCOMMONPROPERTIES commonProperties,
            /* [annotation][in] */ 
            _In_  ULONG intPropertyCount,
            /* [annotation][in] */ 
            _In_  ULONG devPropertyCount,
            /* [annotation][size_is][in] */ 
            _In_opt_  const DEVPROPERTY *interfaceDevProperties,
            /* [annotation][size_is][in] */ 
            _In_opt_  const DEVPROPERTY *deviceDevProperties,
            /* [annotation][in] */ 
            _In_opt_  const SW_DEVICE_CREATE_INFO *createInfo,
            /* [annotation][string][out] */ 
            _Out_opt_  LPWSTR *createdEndpointDeviceInterfaceId);
        
        DECLSPEC_XFGVIRT(IMidiDeviceManagerInterface, UpdateEndpointProperties)
        HRESULT ( STDMETHODCALLTYPE *UpdateEndpointProperties )( 
            IMidiDeviceManagerInterface * This,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR endpointDeviceInterfaceId,
            /* [annotation][in] */ 
            _In_  ULONG intPropertyCount,
            /* [annotation][size_is][in] */ 
            _In_  const DEVPROPERTY *interfaceDevProperties);
        
        DECLSPEC_XFGVIRT(IMidiDeviceManagerInterface, DeleteEndpointProperties)
        HRESULT ( STDMETHODCALLTYPE *DeleteEndpointProperties )( 
            IMidiDeviceManagerInterface * This,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR endpointDeviceInterfaceId,
            /* [annotation][in] */ 
            _In_  ULONG devPropertyCount,
            /* [annotation][size_is][in] */ 
            _In_opt_  const DEVPROPERTY *devPropKeys);
        
        DECLSPEC_XFGVIRT(IMidiDeviceManagerInterface, DeactivateEndpoint)
        HRESULT ( STDMETHODCALLTYPE *DeactivateEndpoint )( 
            IMidiDeviceManagerInterface * This,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR instanceId);
        
        DECLSPEC_XFGVIRT(IMidiDeviceManagerInterface, RemoveEndpoint)
        HRESULT ( STDMETHODCALLTYPE *RemoveEndpoint )( 
            IMidiDeviceManagerInterface * This,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR instanceId);
        
        DECLSPEC_XFGVIRT(IMidiDeviceManagerInterface, UpdateTransportConfiguration)
        HRESULT ( STDMETHODCALLTYPE *UpdateTransportConfiguration )( 
            IMidiDeviceManagerInterface * This,
            /* [annotation][in] */ 
            _In_  GUID transportId,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR configurationJson,
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *responseJson);
        
        END_INTERFACE
    } IMidiDeviceManagerInterfaceVtbl;

    interface IMidiDeviceManagerInterface
    {
        CONST_VTBL struct IMidiDeviceManagerInterfaceVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiDeviceManagerInterface_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiDeviceManagerInterface_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiDeviceManagerInterface_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiDeviceManagerInterface_ActivateVirtualParentDevice(This,devPropertyCount,devProperties,createInfo,createdDeviceId)	\
    ( (This)->lpVtbl -> ActivateVirtualParentDevice(This,devPropertyCount,devProperties,createInfo,createdDeviceId) ) 

#define IMidiDeviceManagerInterface_ActivateEndpoint(This,parentInstanceId,umpOnly,midiFlow,commonProperties,intPropertyCount,devPropertyCount,interfaceDevProperties,deviceDevProperties,createInfo,createdEndpointDeviceInterfaceId)	\
    ( (This)->lpVtbl -> ActivateEndpoint(This,parentInstanceId,umpOnly,midiFlow,commonProperties,intPropertyCount,devPropertyCount,interfaceDevProperties,deviceDevProperties,createInfo,createdEndpointDeviceInterfaceId) ) 

#define IMidiDeviceManagerInterface_UpdateEndpointProperties(This,endpointDeviceInterfaceId,intPropertyCount,interfaceDevProperties)	\
    ( (This)->lpVtbl -> UpdateEndpointProperties(This,endpointDeviceInterfaceId,intPropertyCount,interfaceDevProperties) ) 

#define IMidiDeviceManagerInterface_DeleteEndpointProperties(This,endpointDeviceInterfaceId,devPropertyCount,devPropKeys)	\
    ( (This)->lpVtbl -> DeleteEndpointProperties(This,endpointDeviceInterfaceId,devPropertyCount,devPropKeys) ) 

#define IMidiDeviceManagerInterface_DeactivateEndpoint(This,instanceId)	\
    ( (This)->lpVtbl -> DeactivateEndpoint(This,instanceId) ) 

#define IMidiDeviceManagerInterface_RemoveEndpoint(This,instanceId)	\
    ( (This)->lpVtbl -> RemoveEndpoint(This,instanceId) ) 

#define IMidiDeviceManagerInterface_UpdateTransportConfiguration(This,transportId,configurationJson,responseJson)	\
    ( (This)->lpVtbl -> UpdateTransportConfiguration(This,transportId,configurationJson,responseJson) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiDeviceManagerInterface_INTERFACE_DEFINED__ */


#ifndef __IMidiServiceConfigurationManagerInterface_INTERFACE_DEFINED__
#define __IMidiServiceConfigurationManagerInterface_INTERFACE_DEFINED__

/* interface IMidiServiceConfigurationManagerInterface */
/* [uuid][local][object] */ 


EXTERN_C const IID IID_IMidiServiceConfigurationManagerInterface;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("6c21fcda-051b-4f06-8d40-f5bced5957c0")
    IMidiServiceConfigurationManagerInterface : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetTransportCreateActionEntry( 
            /* [annotation][string][in] */ 
            _In_  LPCWSTR sourceTransportJson,
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *responseJson) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetTransportUpdateActionEntry( 
            /* [annotation][string][in] */ 
            _In_  LPCWSTR sourceTransportJson,
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *responseJson) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetTransportRemoveActionEntry( 
            /* [annotation][string][in] */ 
            _In_  LPCWSTR sourceTransportJson,
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *responseJson) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMatchingEndpointEntry( 
            /* [annotation][string][in] */ 
            _In_  LPCWSTR sourceActionObjectJson,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR searchKeyValuePairsJson,
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *responseJson) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetCachedEndpointUpdateEntry( 
            /* [annotation][in] */ 
            _In_  GUID transportId,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR searchKeyValuePairsJson,
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *responseJson) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiServiceConfigurationManagerInterfaceVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiServiceConfigurationManagerInterface * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiServiceConfigurationManagerInterface * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiServiceConfigurationManagerInterface * This);
        
        DECLSPEC_XFGVIRT(IMidiServiceConfigurationManagerInterface, GetTransportCreateActionEntry)
        HRESULT ( STDMETHODCALLTYPE *GetTransportCreateActionEntry )( 
            IMidiServiceConfigurationManagerInterface * This,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR sourceTransportJson,
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *responseJson);
        
        DECLSPEC_XFGVIRT(IMidiServiceConfigurationManagerInterface, GetTransportUpdateActionEntry)
        HRESULT ( STDMETHODCALLTYPE *GetTransportUpdateActionEntry )( 
            IMidiServiceConfigurationManagerInterface * This,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR sourceTransportJson,
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *responseJson);
        
        DECLSPEC_XFGVIRT(IMidiServiceConfigurationManagerInterface, GetTransportRemoveActionEntry)
        HRESULT ( STDMETHODCALLTYPE *GetTransportRemoveActionEntry )( 
            IMidiServiceConfigurationManagerInterface * This,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR sourceTransportJson,
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *responseJson);
        
        DECLSPEC_XFGVIRT(IMidiServiceConfigurationManagerInterface, GetMatchingEndpointEntry)
        HRESULT ( STDMETHODCALLTYPE *GetMatchingEndpointEntry )( 
            IMidiServiceConfigurationManagerInterface * This,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR sourceActionObjectJson,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR searchKeyValuePairsJson,
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *responseJson);
        
        DECLSPEC_XFGVIRT(IMidiServiceConfigurationManagerInterface, GetCachedEndpointUpdateEntry)
        HRESULT ( STDMETHODCALLTYPE *GetCachedEndpointUpdateEntry )( 
            IMidiServiceConfigurationManagerInterface * This,
            /* [annotation][in] */ 
            _In_  GUID transportId,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR searchKeyValuePairsJson,
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *responseJson);
        
        END_INTERFACE
    } IMidiServiceConfigurationManagerInterfaceVtbl;

    interface IMidiServiceConfigurationManagerInterface
    {
        CONST_VTBL struct IMidiServiceConfigurationManagerInterfaceVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiServiceConfigurationManagerInterface_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiServiceConfigurationManagerInterface_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiServiceConfigurationManagerInterface_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiServiceConfigurationManagerInterface_GetTransportCreateActionEntry(This,sourceTransportJson,responseJson)	\
    ( (This)->lpVtbl -> GetTransportCreateActionEntry(This,sourceTransportJson,responseJson) ) 

#define IMidiServiceConfigurationManagerInterface_GetTransportUpdateActionEntry(This,sourceTransportJson,responseJson)	\
    ( (This)->lpVtbl -> GetTransportUpdateActionEntry(This,sourceTransportJson,responseJson) ) 

#define IMidiServiceConfigurationManagerInterface_GetTransportRemoveActionEntry(This,sourceTransportJson,responseJson)	\
    ( (This)->lpVtbl -> GetTransportRemoveActionEntry(This,sourceTransportJson,responseJson) ) 

#define IMidiServiceConfigurationManagerInterface_GetMatchingEndpointEntry(This,sourceActionObjectJson,searchKeyValuePairsJson,responseJson)	\
    ( (This)->lpVtbl -> GetMatchingEndpointEntry(This,sourceActionObjectJson,searchKeyValuePairsJson,responseJson) ) 

#define IMidiServiceConfigurationManagerInterface_GetCachedEndpointUpdateEntry(This,transportId,searchKeyValuePairsJson,responseJson)	\
    ( (This)->lpVtbl -> GetCachedEndpointUpdateEntry(This,transportId,searchKeyValuePairsJson,responseJson) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiServiceConfigurationManagerInterface_INTERFACE_DEFINED__ */


#ifndef __IMidiTransportConfigurationManager_INTERFACE_DEFINED__
#define __IMidiTransportConfigurationManager_INTERFACE_DEFINED__

/* interface IMidiTransportConfigurationManager */
/* [uuid][local][object] */ 


EXTERN_C const IID IID_IMidiTransportConfigurationManager;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("f19dd642-1809-4497-9eee-f230b11bd6fb")
    IMidiTransportConfigurationManager : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( 
            /* [annotation][in] */ 
            _In_  GUID transportId,
            /* [annotation][in] */ 
            _In_opt_  IMidiDeviceManagerInterface *midiDeviceManager,
            /* [annotation][in] */ 
            _In_opt_  IMidiServiceConfigurationManagerInterface *midiServiceConfigurationManager) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE UpdateConfiguration( 
            /* [annotation][string][in] */ 
            _In_  LPCWSTR configurationJsonSection,
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *responseJson) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Shutdown( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiTransportConfigurationManagerVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiTransportConfigurationManager * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiTransportConfigurationManager * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiTransportConfigurationManager * This);
        
        DECLSPEC_XFGVIRT(IMidiTransportConfigurationManager, Initialize)
        HRESULT ( STDMETHODCALLTYPE *Initialize )( 
            IMidiTransportConfigurationManager * This,
            /* [annotation][in] */ 
            _In_  GUID transportId,
            /* [annotation][in] */ 
            _In_opt_  IMidiDeviceManagerInterface *midiDeviceManager,
            /* [annotation][in] */ 
            _In_opt_  IMidiServiceConfigurationManagerInterface *midiServiceConfigurationManager);
        
        DECLSPEC_XFGVIRT(IMidiTransportConfigurationManager, UpdateConfiguration)
        HRESULT ( STDMETHODCALLTYPE *UpdateConfiguration )( 
            IMidiTransportConfigurationManager * This,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR configurationJsonSection,
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *responseJson);
        
        DECLSPEC_XFGVIRT(IMidiTransportConfigurationManager, Shutdown)
        HRESULT ( STDMETHODCALLTYPE *Shutdown )( 
            IMidiTransportConfigurationManager * This);
        
        END_INTERFACE
    } IMidiTransportConfigurationManagerVtbl;

    interface IMidiTransportConfigurationManager
    {
        CONST_VTBL struct IMidiTransportConfigurationManagerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiTransportConfigurationManager_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiTransportConfigurationManager_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiTransportConfigurationManager_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiTransportConfigurationManager_Initialize(This,transportId,midiDeviceManager,midiServiceConfigurationManager)	\
    ( (This)->lpVtbl -> Initialize(This,transportId,midiDeviceManager,midiServiceConfigurationManager) ) 

#define IMidiTransportConfigurationManager_UpdateConfiguration(This,configurationJsonSection,responseJson)	\
    ( (This)->lpVtbl -> UpdateConfiguration(This,configurationJsonSection,responseJson) ) 

#define IMidiTransportConfigurationManager_Shutdown(This)	\
    ( (This)->lpVtbl -> Shutdown(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiTransportConfigurationManager_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_WindowsMidiServices_0000_0008 */
/* [local] */ 

typedef /* [public] */ struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0008_0001
    {
    MidiDataFormats DataFormatIn;
    MidiDataFormats DataFormatOut;
    BYTE UmpGroupIndex;
    } 	TRANSFORMCREATIONPARAMS;

typedef struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0008_0001 *PTRANSFORMCREATIONPARAMS;



extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0008_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0008_v0_0_s_ifspec;

#ifndef __IMidiTransform_INTERFACE_DEFINED__
#define __IMidiTransform_INTERFACE_DEFINED__

/* interface IMidiTransform */
/* [uuid][local][object] */ 


EXTERN_C const IID IID_IMidiTransform;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("65FA86A4-0433-4DCD-88E4-E565051CAB2D")
    IMidiTransform : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Activate( 
            /* [annotation][in] */ 
            _In_  REFIID iid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **activatedInterface) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiTransformVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiTransform * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiTransform * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiTransform * This);
        
        DECLSPEC_XFGVIRT(IMidiTransform, Activate)
        HRESULT ( STDMETHODCALLTYPE *Activate )( 
            IMidiTransform * This,
            /* [annotation][in] */ 
            _In_  REFIID iid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **activatedInterface);
        
        END_INTERFACE
    } IMidiTransformVtbl;

    interface IMidiTransform
    {
        CONST_VTBL struct IMidiTransformVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiTransform_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiTransform_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiTransform_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiTransform_Activate(This,iid,activatedInterface)	\
    ( (This)->lpVtbl -> Activate(This,iid,activatedInterface) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiTransform_INTERFACE_DEFINED__ */


#ifndef __IMidiDataTransform_INTERFACE_DEFINED__
#define __IMidiDataTransform_INTERFACE_DEFINED__

/* interface IMidiDataTransform */
/* [uuid][local][object] */ 


EXTERN_C const IID IID_IMidiDataTransform;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("5D2400F0-F2EE-4A51-A3BE-0AC9A19C09C4")
    IMidiDataTransform : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( 
            /* [annotation][string][in] */ 
            _In_  LPCWSTR endpointDeviceInterfaceId,
            /* [annotation][in] */ 
            _In_  PTRANSFORMCREATIONPARAMS creationParams,
            /* [annotation][out][in] */ 
            _Inout_  DWORD *mmcssTaskId,
            /* [annotation][in] */ 
            _In_  IMidiCallback *callback,
            /* [annotation][in] */ 
            _In_  LONGLONG context,
            /* [annotation][in] */ 
            _In_  IMidiDeviceManagerInterface *midiDeviceManager) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Shutdown( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendMidiMessage( 
            /* [annotation][in] */ 
            _In_  PVOID message,
            /* [annotation][in] */ 
            _In_  UINT size,
            /* [annotation][in] */ 
            _In_  LONGLONG position) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiDataTransformVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiDataTransform * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiDataTransform * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiDataTransform * This);
        
        DECLSPEC_XFGVIRT(IMidiDataTransform, Initialize)
        HRESULT ( STDMETHODCALLTYPE *Initialize )( 
            IMidiDataTransform * This,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR endpointDeviceInterfaceId,
            /* [annotation][in] */ 
            _In_  PTRANSFORMCREATIONPARAMS creationParams,
            /* [annotation][out][in] */ 
            _Inout_  DWORD *mmcssTaskId,
            /* [annotation][in] */ 
            _In_  IMidiCallback *callback,
            /* [annotation][in] */ 
            _In_  LONGLONG context,
            /* [annotation][in] */ 
            _In_  IMidiDeviceManagerInterface *midiDeviceManager);
        
        DECLSPEC_XFGVIRT(IMidiDataTransform, Shutdown)
        HRESULT ( STDMETHODCALLTYPE *Shutdown )( 
            IMidiDataTransform * This);
        
        DECLSPEC_XFGVIRT(IMidiDataTransform, SendMidiMessage)
        HRESULT ( STDMETHODCALLTYPE *SendMidiMessage )( 
            IMidiDataTransform * This,
            /* [annotation][in] */ 
            _In_  PVOID message,
            /* [annotation][in] */ 
            _In_  UINT size,
            /* [annotation][in] */ 
            _In_  LONGLONG position);
        
        END_INTERFACE
    } IMidiDataTransformVtbl;

    interface IMidiDataTransform
    {
        CONST_VTBL struct IMidiDataTransformVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiDataTransform_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiDataTransform_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiDataTransform_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiDataTransform_Initialize(This,endpointDeviceInterfaceId,creationParams,mmcssTaskId,callback,context,midiDeviceManager)	\
    ( (This)->lpVtbl -> Initialize(This,endpointDeviceInterfaceId,creationParams,mmcssTaskId,callback,context,midiDeviceManager) ) 

#define IMidiDataTransform_Shutdown(This)	\
    ( (This)->lpVtbl -> Shutdown(This) ) 

#define IMidiDataTransform_SendMidiMessage(This,message,size,position)	\
    ( (This)->lpVtbl -> SendMidiMessage(This,message,size,position) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiDataTransform_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_WindowsMidiServices_0000_0010 */
/* [local] */ 

typedef /* [public] */ struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0010_0001
    {
    BYTE Number;
    BYTE FirstGroup;
    BYTE NumberOfGroupsSpanned;
    BOOL IsActive;
    BOOL IsMIDIMessageDestination;
    BOOL IsMIDIMessageSource;
    LPCWSTR Name;
    } 	DISCOVEREDFUNCTIONBLOCK;

typedef struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0010_0001 *PDISCOVEREDFUNCTIONBLOCK;

typedef /* [public] */ struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0010_0002
    {
    BOOL AllEndpointInformationReceived;
    MidiEndpointCapabilities EndpointCapabilities;
    LPCWSTR EndpointSuppliedName;
    LPCWSTR EndpointSuppliedProductInstanceId;
    BYTE CountFunctionBlocksDeclared;
    BYTE CountFunctionBlocksReceived;
    BOOL FunctionBlocksAreStatic;
    PDISCOVEREDFUNCTIONBLOCK DiscoveredFunctionBlocks;
    } 	ENDPOINTPROTOCOLNEGOTIATIONRESULTS;

typedef struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0010_0002 *PENDPOINTPROTOCOLNEGOTIATIONRESULTS;

typedef /* [public][public] */ struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0010_0003
    {
    BOOL PreferToSendJitterReductionTimestampsToEndpoint;
    BOOL PreferToReceiveJitterReductionTimestampsFromEndpoint;
    BYTE PreferredMidiProtocol;
    WORD TimeoutMilliseconds;
    } 	ENDPOINTPROTOCOLNEGOTIATIONPARAMS;

typedef struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0010_0003 *PENDPOINTPROTOCOLNEGOTIATIONPARAMS;



extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0010_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0010_v0_0_s_ifspec;

#ifndef __IMidiProtocolNegotiationCompleteCallback_INTERFACE_DEFINED__
#define __IMidiProtocolNegotiationCompleteCallback_INTERFACE_DEFINED__

/* interface IMidiProtocolNegotiationCompleteCallback */
/* [uuid][local][object] */ 


EXTERN_C const IID IID_IMidiProtocolNegotiationCompleteCallback;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("f948dc64-e03a-4e24-bc6e-437ad729cd50")
    IMidiProtocolNegotiationCompleteCallback : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE ProtocolNegotiationCompleteCallback( 
            /* [annotation][in] */ 
            _In_  GUID transportId,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR endpointDeviceInterfaceId,
            /* [annotation][in] */ 
            _In_  PENDPOINTPROTOCOLNEGOTIATIONRESULTS results) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiProtocolNegotiationCompleteCallbackVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiProtocolNegotiationCompleteCallback * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiProtocolNegotiationCompleteCallback * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiProtocolNegotiationCompleteCallback * This);
        
        DECLSPEC_XFGVIRT(IMidiProtocolNegotiationCompleteCallback, ProtocolNegotiationCompleteCallback)
        HRESULT ( STDMETHODCALLTYPE *ProtocolNegotiationCompleteCallback )( 
            IMidiProtocolNegotiationCompleteCallback * This,
            /* [annotation][in] */ 
            _In_  GUID transportId,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR endpointDeviceInterfaceId,
            /* [annotation][in] */ 
            _In_  PENDPOINTPROTOCOLNEGOTIATIONRESULTS results);
        
        END_INTERFACE
    } IMidiProtocolNegotiationCompleteCallbackVtbl;

    interface IMidiProtocolNegotiationCompleteCallback
    {
        CONST_VTBL struct IMidiProtocolNegotiationCompleteCallbackVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiProtocolNegotiationCompleteCallback_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiProtocolNegotiationCompleteCallback_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiProtocolNegotiationCompleteCallback_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiProtocolNegotiationCompleteCallback_ProtocolNegotiationCompleteCallback(This,transportId,endpointDeviceInterfaceId,results)	\
    ( (This)->lpVtbl -> ProtocolNegotiationCompleteCallback(This,transportId,endpointDeviceInterfaceId,results) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiProtocolNegotiationCompleteCallback_INTERFACE_DEFINED__ */


#ifndef __IMidiEndpointProtocolManagerInterface_INTERFACE_DEFINED__
#define __IMidiEndpointProtocolManagerInterface_INTERFACE_DEFINED__

/* interface IMidiEndpointProtocolManagerInterface */
/* [uuid][local][object] */ 


EXTERN_C const IID IID_IMidiEndpointProtocolManagerInterface;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("7a3def20-fb76-49b6-a3a0-3dcbcda8f482")
    IMidiEndpointProtocolManagerInterface : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE DiscoverAndNegotiate( 
            /* [annotation][in] */ 
            _In_  GUID transportId,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR endpointDeviceInterfaceId,
            /* [annotation][in] */ 
            _In_  ENDPOINTPROTOCOLNEGOTIATIONPARAMS negotiationParams,
            /* [annotation][in] */ 
            _In_  IMidiProtocolNegotiationCompleteCallback *negotiationCompleteCallback) = 0;
        
        virtual BOOL STDMETHODCALLTYPE IsEnabled( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiEndpointProtocolManagerInterfaceVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiEndpointProtocolManagerInterface * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiEndpointProtocolManagerInterface * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiEndpointProtocolManagerInterface * This);
        
        DECLSPEC_XFGVIRT(IMidiEndpointProtocolManagerInterface, DiscoverAndNegotiate)
        HRESULT ( STDMETHODCALLTYPE *DiscoverAndNegotiate )( 
            IMidiEndpointProtocolManagerInterface * This,
            /* [annotation][in] */ 
            _In_  GUID transportId,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR endpointDeviceInterfaceId,
            /* [annotation][in] */ 
            _In_  ENDPOINTPROTOCOLNEGOTIATIONPARAMS negotiationParams,
            /* [annotation][in] */ 
            _In_  IMidiProtocolNegotiationCompleteCallback *negotiationCompleteCallback);
        
        DECLSPEC_XFGVIRT(IMidiEndpointProtocolManagerInterface, IsEnabled)
        BOOL ( STDMETHODCALLTYPE *IsEnabled )( 
            IMidiEndpointProtocolManagerInterface * This);
        
        END_INTERFACE
    } IMidiEndpointProtocolManagerInterfaceVtbl;

    interface IMidiEndpointProtocolManagerInterface
    {
        CONST_VTBL struct IMidiEndpointProtocolManagerInterfaceVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiEndpointProtocolManagerInterface_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiEndpointProtocolManagerInterface_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiEndpointProtocolManagerInterface_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiEndpointProtocolManagerInterface_DiscoverAndNegotiate(This,transportId,endpointDeviceInterfaceId,negotiationParams,negotiationCompleteCallback)	\
    ( (This)->lpVtbl -> DiscoverAndNegotiate(This,transportId,endpointDeviceInterfaceId,negotiationParams,negotiationCompleteCallback) ) 

#define IMidiEndpointProtocolManagerInterface_IsEnabled(This)	\
    ( (This)->lpVtbl -> IsEnabled(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiEndpointProtocolManagerInterface_INTERFACE_DEFINED__ */


#ifndef __IMidiEndpointManager_INTERFACE_DEFINED__
#define __IMidiEndpointManager_INTERFACE_DEFINED__

/* interface IMidiEndpointManager */
/* [uuid][local][object] */ 


EXTERN_C const IID IID_IMidiEndpointManager;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("badff6d2-0e3c-4009-a473-6ba82c008662")
    IMidiEndpointManager : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( 
            /* [annotation][in] */ 
            _In_  IMidiDeviceManagerInterface *midiDeviceManager,
            /* [annotation][in] */ 
            _In_  IMidiEndpointProtocolManagerInterface *midiEndpointProtocolManager) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Shutdown( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiEndpointManagerVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiEndpointManager * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiEndpointManager * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiEndpointManager * This);
        
        DECLSPEC_XFGVIRT(IMidiEndpointManager, Initialize)
        HRESULT ( STDMETHODCALLTYPE *Initialize )( 
            IMidiEndpointManager * This,
            /* [annotation][in] */ 
            _In_  IMidiDeviceManagerInterface *midiDeviceManager,
            /* [annotation][in] */ 
            _In_  IMidiEndpointProtocolManagerInterface *midiEndpointProtocolManager);
        
        DECLSPEC_XFGVIRT(IMidiEndpointManager, Shutdown)
        HRESULT ( STDMETHODCALLTYPE *Shutdown )( 
            IMidiEndpointManager * This);
        
        END_INTERFACE
    } IMidiEndpointManagerVtbl;

    interface IMidiEndpointManager
    {
        CONST_VTBL struct IMidiEndpointManagerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiEndpointManager_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiEndpointManager_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiEndpointManager_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiEndpointManager_Initialize(This,midiDeviceManager,midiEndpointProtocolManager)	\
    ( (This)->lpVtbl -> Initialize(This,midiDeviceManager,midiEndpointProtocolManager) ) 

#define IMidiEndpointManager_Shutdown(This)	\
    ( (This)->lpVtbl -> Shutdown(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiEndpointManager_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_WindowsMidiServices_0000_0013 */
/* [local] */ 

typedef /* [public] */ struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0013_0001
    {
    GUID TransportId;
    LPWSTR TransportCode;
    LPWSTR Name;
    LPWSTR Description;
    LPWSTR Author;
    LPWSTR SmallImagePath;
    LPWSTR Version;
    MetadataFlags Flags;
    } 	TRANSPORTMETADATA;

typedef struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0013_0001 *PTRANSPORTMETADATA;

typedef /* [public] */ struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0013_0002
    {
    GUID TransformId;
    LPWSTR Name;
    LPWSTR Description;
    LPWSTR Author;
    LPWSTR SmallImagePath;
    LPWSTR Version;
    MetadataFlags Flags;
    } 	TRANSFORMMETADATA;

typedef struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0013_0002 *PTRANSFORMMETADATA;



extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0013_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0013_v0_0_s_ifspec;

#ifndef __IMidiServiceTransportPluginMetadataProvider_INTERFACE_DEFINED__
#define __IMidiServiceTransportPluginMetadataProvider_INTERFACE_DEFINED__

/* interface IMidiServiceTransportPluginMetadataProvider */
/* [uuid][local][object] */ 


EXTERN_C const IID IID_IMidiServiceTransportPluginMetadataProvider;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8d542740-83f8-4a9a-8627-cd6324b6c1f4")
    IMidiServiceTransportPluginMetadataProvider : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMetadata( 
            /* [annotation][out] */ 
            _Out_  PTRANSPORTMETADATA metadata) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Shutdown( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiServiceTransportPluginMetadataProviderVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiServiceTransportPluginMetadataProvider * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiServiceTransportPluginMetadataProvider * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiServiceTransportPluginMetadataProvider * This);
        
        DECLSPEC_XFGVIRT(IMidiServiceTransportPluginMetadataProvider, Initialize)
        HRESULT ( STDMETHODCALLTYPE *Initialize )( 
            IMidiServiceTransportPluginMetadataProvider * This);
        
        DECLSPEC_XFGVIRT(IMidiServiceTransportPluginMetadataProvider, GetMetadata)
        HRESULT ( STDMETHODCALLTYPE *GetMetadata )( 
            IMidiServiceTransportPluginMetadataProvider * This,
            /* [annotation][out] */ 
            _Out_  PTRANSPORTMETADATA metadata);
        
        DECLSPEC_XFGVIRT(IMidiServiceTransportPluginMetadataProvider, Shutdown)
        HRESULT ( STDMETHODCALLTYPE *Shutdown )( 
            IMidiServiceTransportPluginMetadataProvider * This);
        
        END_INTERFACE
    } IMidiServiceTransportPluginMetadataProviderVtbl;

    interface IMidiServiceTransportPluginMetadataProvider
    {
        CONST_VTBL struct IMidiServiceTransportPluginMetadataProviderVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiServiceTransportPluginMetadataProvider_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiServiceTransportPluginMetadataProvider_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiServiceTransportPluginMetadataProvider_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiServiceTransportPluginMetadataProvider_Initialize(This)	\
    ( (This)->lpVtbl -> Initialize(This) ) 

#define IMidiServiceTransportPluginMetadataProvider_GetMetadata(This,metadata)	\
    ( (This)->lpVtbl -> GetMetadata(This,metadata) ) 

#define IMidiServiceTransportPluginMetadataProvider_Shutdown(This)	\
    ( (This)->lpVtbl -> Shutdown(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiServiceTransportPluginMetadataProvider_INTERFACE_DEFINED__ */


#ifndef __IMidiServiceTransformPluginMetadataProvider_INTERFACE_DEFINED__
#define __IMidiServiceTransformPluginMetadataProvider_INTERFACE_DEFINED__

/* interface IMidiServiceTransformPluginMetadataProvider */
/* [uuid][local][object] */ 


EXTERN_C const IID IID_IMidiServiceTransformPluginMetadataProvider;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("76c65f88-061f-4e4f-bb9d-3ac1981f12f2")
    IMidiServiceTransformPluginMetadataProvider : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMetadata( 
            /* [annotation][out] */ 
            _Out_  PTRANSFORMMETADATA metadata) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Shutdown( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiServiceTransformPluginMetadataProviderVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiServiceTransformPluginMetadataProvider * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiServiceTransformPluginMetadataProvider * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiServiceTransformPluginMetadataProvider * This);
        
        DECLSPEC_XFGVIRT(IMidiServiceTransformPluginMetadataProvider, Initialize)
        HRESULT ( STDMETHODCALLTYPE *Initialize )( 
            IMidiServiceTransformPluginMetadataProvider * This);
        
        DECLSPEC_XFGVIRT(IMidiServiceTransformPluginMetadataProvider, GetMetadata)
        HRESULT ( STDMETHODCALLTYPE *GetMetadata )( 
            IMidiServiceTransformPluginMetadataProvider * This,
            /* [annotation][out] */ 
            _Out_  PTRANSFORMMETADATA metadata);
        
        DECLSPEC_XFGVIRT(IMidiServiceTransformPluginMetadataProvider, Shutdown)
        HRESULT ( STDMETHODCALLTYPE *Shutdown )( 
            IMidiServiceTransformPluginMetadataProvider * This);
        
        END_INTERFACE
    } IMidiServiceTransformPluginMetadataProviderVtbl;

    interface IMidiServiceTransformPluginMetadataProvider
    {
        CONST_VTBL struct IMidiServiceTransformPluginMetadataProviderVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiServiceTransformPluginMetadataProvider_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiServiceTransformPluginMetadataProvider_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiServiceTransformPluginMetadataProvider_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiServiceTransformPluginMetadataProvider_Initialize(This)	\
    ( (This)->lpVtbl -> Initialize(This) ) 

#define IMidiServiceTransformPluginMetadataProvider_GetMetadata(This,metadata)	\
    ( (This)->lpVtbl -> GetMetadata(This,metadata) ) 

#define IMidiServiceTransformPluginMetadataProvider_Shutdown(This)	\
    ( (This)->lpVtbl -> Shutdown(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiServiceTransformPluginMetadataProvider_INTERFACE_DEFINED__ */


#ifndef __IMidiServicePluginMetadataReporterInterface_INTERFACE_DEFINED__
#define __IMidiServicePluginMetadataReporterInterface_INTERFACE_DEFINED__

/* interface IMidiServicePluginMetadataReporterInterface */
/* [uuid][local][object] */ 


EXTERN_C const IID IID_IMidiServicePluginMetadataReporterInterface;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("469c7722-f779-40c3-b648-52620f75dcee")
    IMidiServicePluginMetadataReporterInterface : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetTransportList( 
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *transportListJson) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetTransformList( 
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *transformListJson) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiServicePluginMetadataReporterInterfaceVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiServicePluginMetadataReporterInterface * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiServicePluginMetadataReporterInterface * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiServicePluginMetadataReporterInterface * This);
        
        DECLSPEC_XFGVIRT(IMidiServicePluginMetadataReporterInterface, GetTransportList)
        HRESULT ( STDMETHODCALLTYPE *GetTransportList )( 
            IMidiServicePluginMetadataReporterInterface * This,
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *transportListJson);
        
        DECLSPEC_XFGVIRT(IMidiServicePluginMetadataReporterInterface, GetTransformList)
        HRESULT ( STDMETHODCALLTYPE *GetTransformList )( 
            IMidiServicePluginMetadataReporterInterface * This,
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *transformListJson);
        
        END_INTERFACE
    } IMidiServicePluginMetadataReporterInterfaceVtbl;

    interface IMidiServicePluginMetadataReporterInterface
    {
        CONST_VTBL struct IMidiServicePluginMetadataReporterInterfaceVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiServicePluginMetadataReporterInterface_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiServicePluginMetadataReporterInterface_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiServicePluginMetadataReporterInterface_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiServicePluginMetadataReporterInterface_GetTransportList(This,transportListJson)	\
    ( (This)->lpVtbl -> GetTransportList(This,transportListJson) ) 

#define IMidiServicePluginMetadataReporterInterface_GetTransformList(This,transformListJson)	\
    ( (This)->lpVtbl -> GetTransformList(This,transformListJson) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiServicePluginMetadataReporterInterface_INTERFACE_DEFINED__ */


#ifndef __IMidiSessionTracker_INTERFACE_DEFINED__
#define __IMidiSessionTracker_INTERFACE_DEFINED__

/* interface IMidiSessionTracker */
/* [uuid][local][object] */ 


EXTERN_C const IID IID_IMidiSessionTracker;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("194c2746-3ae5-419a-94d9-20416c7dbefe")
    IMidiSessionTracker : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE AddClientSession( 
            /* [annotation][in] */ 
            _In_  GUID sessionId,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR sessionName) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE UpdateClientSessionName( 
            /* [annotation][in] */ 
            _In_  GUID sessionId,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR sessionName) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE RemoveClientSession( 
            /* [annotation][in] */ 
            _In_  GUID sessionId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetSessionList( 
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *sessionDetailsList) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Shutdown( void) = 0;
        
        virtual BOOL STDMETHODCALLTYPE VerifyConnectivity( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiSessionTrackerVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiSessionTracker * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiSessionTracker * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiSessionTracker * This);
        
        DECLSPEC_XFGVIRT(IMidiSessionTracker, Initialize)
        HRESULT ( STDMETHODCALLTYPE *Initialize )( 
            IMidiSessionTracker * This);
        
        DECLSPEC_XFGVIRT(IMidiSessionTracker, AddClientSession)
        HRESULT ( STDMETHODCALLTYPE *AddClientSession )( 
            IMidiSessionTracker * This,
            /* [annotation][in] */ 
            _In_  GUID sessionId,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR sessionName);
        
        DECLSPEC_XFGVIRT(IMidiSessionTracker, UpdateClientSessionName)
        HRESULT ( STDMETHODCALLTYPE *UpdateClientSessionName )( 
            IMidiSessionTracker * This,
            /* [annotation][in] */ 
            _In_  GUID sessionId,
            /* [annotation][string][in] */ 
            _In_  LPCWSTR sessionName);
        
        DECLSPEC_XFGVIRT(IMidiSessionTracker, RemoveClientSession)
        HRESULT ( STDMETHODCALLTYPE *RemoveClientSession )( 
            IMidiSessionTracker * This,
            /* [annotation][in] */ 
            _In_  GUID sessionId);
        
        DECLSPEC_XFGVIRT(IMidiSessionTracker, GetSessionList)
        HRESULT ( STDMETHODCALLTYPE *GetSessionList )( 
            IMidiSessionTracker * This,
            /* [annotation][string][out] */ 
            _Out_  LPWSTR *sessionDetailsList);
        
        DECLSPEC_XFGVIRT(IMidiSessionTracker, Shutdown)
        HRESULT ( STDMETHODCALLTYPE *Shutdown )( 
            IMidiSessionTracker * This);
        
        DECLSPEC_XFGVIRT(IMidiSessionTracker, VerifyConnectivity)
        BOOL ( STDMETHODCALLTYPE *VerifyConnectivity )( 
            IMidiSessionTracker * This);
        
        END_INTERFACE
    } IMidiSessionTrackerVtbl;

    interface IMidiSessionTracker
    {
        CONST_VTBL struct IMidiSessionTrackerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiSessionTracker_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiSessionTracker_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiSessionTracker_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiSessionTracker_Initialize(This)	\
    ( (This)->lpVtbl -> Initialize(This) ) 

#define IMidiSessionTracker_AddClientSession(This,sessionId,sessionName)	\
    ( (This)->lpVtbl -> AddClientSession(This,sessionId,sessionName) ) 

#define IMidiSessionTracker_UpdateClientSessionName(This,sessionId,sessionName)	\
    ( (This)->lpVtbl -> UpdateClientSessionName(This,sessionId,sessionName) ) 

#define IMidiSessionTracker_RemoveClientSession(This,sessionId)	\
    ( (This)->lpVtbl -> RemoveClientSession(This,sessionId) ) 

#define IMidiSessionTracker_GetSessionList(This,sessionDetailsList)	\
    ( (This)->lpVtbl -> GetSessionList(This,sessionDetailsList) ) 

#define IMidiSessionTracker_Shutdown(This)	\
    ( (This)->lpVtbl -> Shutdown(This) ) 

#define IMidiSessionTracker_VerifyConnectivity(This)	\
    ( (This)->lpVtbl -> VerifyConnectivity(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiSessionTracker_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


