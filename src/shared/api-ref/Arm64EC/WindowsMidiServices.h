

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

#ifndef __IMidiAbstraction_FWD_DEFINED__
#define __IMidiAbstraction_FWD_DEFINED__
typedef interface IMidiAbstraction IMidiAbstraction;

#endif 	/* __IMidiAbstraction_FWD_DEFINED__ */


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


#ifndef __IMidiEndpointManager_FWD_DEFINED__
#define __IMidiEndpointManager_FWD_DEFINED__
typedef interface IMidiEndpointManager IMidiEndpointManager;

#endif 	/* __IMidiEndpointManager_FWD_DEFINED__ */


#ifndef __IMidiDeviceManagerInterface_FWD_DEFINED__
#define __IMidiDeviceManagerInterface_FWD_DEFINED__
typedef interface IMidiDeviceManagerInterface IMidiDeviceManagerInterface;

#endif 	/* __IMidiDeviceManagerInterface_FWD_DEFINED__ */


#ifndef __IMidiServiceConfigurationManagerInterface_FWD_DEFINED__
#define __IMidiServiceConfigurationManagerInterface_FWD_DEFINED__
typedef interface IMidiServiceConfigurationManagerInterface IMidiServiceConfigurationManagerInterface;

#endif 	/* __IMidiServiceConfigurationManagerInterface_FWD_DEFINED__ */


#ifndef __IMidiAbstractionConfigurationManager_FWD_DEFINED__
#define __IMidiAbstractionConfigurationManager_FWD_DEFINED__
typedef interface IMidiAbstractionConfigurationManager IMidiAbstractionConfigurationManager;

#endif 	/* __IMidiAbstractionConfigurationManager_FWD_DEFINED__ */


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


#ifndef __IMidiServiceAbstractionPluginMetadataProvider_FWD_DEFINED__
#define __IMidiServiceAbstractionPluginMetadataProvider_FWD_DEFINED__
typedef interface IMidiServiceAbstractionPluginMetadataProvider IMidiServiceAbstractionPluginMetadataProvider;

#endif 	/* __IMidiServiceAbstractionPluginMetadataProvider_FWD_DEFINED__ */


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

typedef /* [public][public][public][public][public][public][public][public][public][public][public][public][public] */ 
enum __MIDL___MIDL_itf_WindowsMidiServices_0000_0000_0001
    {
        MidiDataFormat_Invalid	= 0,
        MidiDataFormat_ByteStream	= 0x1,
        MidiDataFormat_UMP	= 0x2,
        MidiDataFormat_Any	= 0x3
    } 	MidiDataFormat;

typedef /* [public][public] */ 
enum __MIDL___MIDL_itf_WindowsMidiServices_0000_0000_0002
    {
        MidiFlowIn	= 0,
        MidiFlowOut	= ( MidiFlowIn + 1 ) ,
        MidiFlowBidirectional	= ( MidiFlowOut + 1 ) 
    } 	MidiFlow;

typedef /* [public] */ struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0000_0003
    {
    MidiDataFormat DataFormat;
    LPCWSTR InstanceConfigurationJsonData;
    } 	ABSTRACTIONCREATIONPARAMS;

typedef struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0000_0003 *PABSTRACTIONCREATIONPARAMS;



extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0000_v0_0_s_ifspec;

#ifndef __IMidiAbstraction_INTERFACE_DEFINED__
#define __IMidiAbstraction_INTERFACE_DEFINED__

/* interface IMidiAbstraction */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMidiAbstraction;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("EA264200-3328-49E5-8815-73649A8748BE")
    IMidiAbstraction : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Activate( 
            /* [annotation][in] */ 
            _In_  REFIID Iid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **Interface) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiAbstractionVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiAbstraction * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiAbstraction * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiAbstraction * This);
        
        DECLSPEC_XFGVIRT(IMidiAbstraction, Activate)
        HRESULT ( STDMETHODCALLTYPE *Activate )( 
            IMidiAbstraction * This,
            /* [annotation][in] */ 
            _In_  REFIID Iid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **Interface);
        
        END_INTERFACE
    } IMidiAbstractionVtbl;

    interface IMidiAbstraction
    {
        CONST_VTBL struct IMidiAbstractionVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiAbstraction_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiAbstraction_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiAbstraction_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiAbstraction_Activate(This,Iid,Interface)	\
    ( (This)->lpVtbl -> Activate(This,Iid,Interface) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiAbstraction_INTERFACE_DEFINED__ */


#ifndef __IMidiCallback_INTERFACE_DEFINED__
#define __IMidiCallback_INTERFACE_DEFINED__

/* interface IMidiCallback */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMidiCallback;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("4D6A29E5-DF4F-4A2D-A923-9B23B3F2D6F6")
    IMidiCallback : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Callback( 
            /* [in] */ PVOID message,
            /* [in] */ UINT size,
            /* [in] */ LONGLONG position,
            /* [in] */ LONGLONG context) = 0;
        
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
            /* [in] */ PVOID message,
            /* [in] */ UINT size,
            /* [in] */ LONGLONG position,
            /* [in] */ LONGLONG context);
        
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
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMidiIn;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("6C4B8BF9-8133-4B41-8303-1FDE78E20ACA")
    IMidiIn : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( 
            /* [in] */ LPCWSTR deviceId,
            /* [in] */ PABSTRACTIONCREATIONPARAMS creationParams,
            /* [in] */ DWORD *mmcssTaskId,
            /* [in] */ IMidiCallback *callback,
            /* [in] */ LONGLONG context,
            /* [in] */ GUID SessionId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Cleanup( void) = 0;
        
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
            /* [in] */ LPCWSTR deviceId,
            /* [in] */ PABSTRACTIONCREATIONPARAMS creationParams,
            /* [in] */ DWORD *mmcssTaskId,
            /* [in] */ IMidiCallback *callback,
            /* [in] */ LONGLONG context,
            /* [in] */ GUID SessionId);
        
        DECLSPEC_XFGVIRT(IMidiIn, Cleanup)
        HRESULT ( STDMETHODCALLTYPE *Cleanup )( 
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


#define IMidiIn_Initialize(This,deviceId,creationParams,mmcssTaskId,callback,context,SessionId)	\
    ( (This)->lpVtbl -> Initialize(This,deviceId,creationParams,mmcssTaskId,callback,context,SessionId) ) 

#define IMidiIn_Cleanup(This)	\
    ( (This)->lpVtbl -> Cleanup(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiIn_INTERFACE_DEFINED__ */


#ifndef __IMidiOut_INTERFACE_DEFINED__
#define __IMidiOut_INTERFACE_DEFINED__

/* interface IMidiOut */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMidiOut;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("F328D645-7D6D-4924-A7E3-9DEE245189F9")
    IMidiOut : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( 
            /* [in] */ LPCWSTR deviceId,
            /* [in] */ PABSTRACTIONCREATIONPARAMS creationParams,
            /* [in] */ DWORD *mmcssTaskId,
            /* [in] */ GUID SessionId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Cleanup( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendMidiMessage( 
            /* [in] */ PVOID message,
            /* [in] */ UINT size,
            /* [in] */ LONGLONG position) = 0;
        
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
            /* [in] */ LPCWSTR deviceId,
            /* [in] */ PABSTRACTIONCREATIONPARAMS creationParams,
            /* [in] */ DWORD *mmcssTaskId,
            /* [in] */ GUID SessionId);
        
        DECLSPEC_XFGVIRT(IMidiOut, Cleanup)
        HRESULT ( STDMETHODCALLTYPE *Cleanup )( 
            IMidiOut * This);
        
        DECLSPEC_XFGVIRT(IMidiOut, SendMidiMessage)
        HRESULT ( STDMETHODCALLTYPE *SendMidiMessage )( 
            IMidiOut * This,
            /* [in] */ PVOID message,
            /* [in] */ UINT size,
            /* [in] */ LONGLONG position);
        
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


#define IMidiOut_Initialize(This,deviceId,creationParams,mmcssTaskId,SessionId)	\
    ( (This)->lpVtbl -> Initialize(This,deviceId,creationParams,mmcssTaskId,SessionId) ) 

#define IMidiOut_Cleanup(This)	\
    ( (This)->lpVtbl -> Cleanup(This) ) 

#define IMidiOut_SendMidiMessage(This,message,size,position)	\
    ( (This)->lpVtbl -> SendMidiMessage(This,message,size,position) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiOut_INTERFACE_DEFINED__ */


#ifndef __IMidiBiDi_INTERFACE_DEFINED__
#define __IMidiBiDi_INTERFACE_DEFINED__

/* interface IMidiBiDi */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMidiBiDi;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("B89BBB45-7001-4BEA-BBD8-C7CC26E7836C")
    IMidiBiDi : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( 
            /* [in] */ LPCWSTR deviceId,
            /* [in] */ PABSTRACTIONCREATIONPARAMS creationParams,
            /* [in] */ DWORD *mmcssTaskId,
            /* [in] */ IMidiCallback *callback,
            /* [in] */ LONGLONG context,
            /* [in] */ GUID SessionId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Cleanup( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendMidiMessage( 
            /* [in] */ PVOID message,
            /* [in] */ UINT size,
            /* [in] */ LONGLONG position) = 0;
        
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
            /* [in] */ LPCWSTR deviceId,
            /* [in] */ PABSTRACTIONCREATIONPARAMS creationParams,
            /* [in] */ DWORD *mmcssTaskId,
            /* [in] */ IMidiCallback *callback,
            /* [in] */ LONGLONG context,
            /* [in] */ GUID SessionId);
        
        DECLSPEC_XFGVIRT(IMidiBiDi, Cleanup)
        HRESULT ( STDMETHODCALLTYPE *Cleanup )( 
            IMidiBiDi * This);
        
        DECLSPEC_XFGVIRT(IMidiBiDi, SendMidiMessage)
        HRESULT ( STDMETHODCALLTYPE *SendMidiMessage )( 
            IMidiBiDi * This,
            /* [in] */ PVOID message,
            /* [in] */ UINT size,
            /* [in] */ LONGLONG position);
        
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


#define IMidiBiDi_Initialize(This,deviceId,creationParams,mmcssTaskId,callback,context,SessionId)	\
    ( (This)->lpVtbl -> Initialize(This,deviceId,creationParams,mmcssTaskId,callback,context,SessionId) ) 

#define IMidiBiDi_Cleanup(This)	\
    ( (This)->lpVtbl -> Cleanup(This) ) 

#define IMidiBiDi_SendMidiMessage(This,message,size,position)	\
    ( (This)->lpVtbl -> SendMidiMessage(This,message,size,position) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiBiDi_INTERFACE_DEFINED__ */


#ifndef __IMidiEndpointManager_INTERFACE_DEFINED__
#define __IMidiEndpointManager_INTERFACE_DEFINED__

/* interface IMidiEndpointManager */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMidiEndpointManager;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("badff6d2-0e3c-4009-a473-6ba82c008662")
    IMidiEndpointManager : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( 
            /* [in] */ IUnknown *midiDeviceManager,
            /* [in] */ IUnknown *midiEndpointProtocolManager) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Cleanup( void) = 0;
        
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
            /* [in] */ IUnknown *midiDeviceManager,
            /* [in] */ IUnknown *midiEndpointProtocolManager);
        
        DECLSPEC_XFGVIRT(IMidiEndpointManager, Cleanup)
        HRESULT ( STDMETHODCALLTYPE *Cleanup )( 
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

#define IMidiEndpointManager_Cleanup(This)	\
    ( (This)->lpVtbl -> Cleanup(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiEndpointManager_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_WindowsMidiServices_0000_0006 */
/* [local] */ 

typedef /* [public][public][public][public] */ 
enum __MIDL___MIDL_itf_WindowsMidiServices_0000_0006_0001
    {
        NormalMessageEndpoint	= 0,
        VirtualDeviceResponder	= 100,
        InBoxGeneralMidiSynth	= 400,
        DiagnosticLoopback	= 500,
        DiagnosticPing	= 510
    } 	MidiEndpointDevicePurposePropertyValue;

typedef /* [public] */ struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0006_0002
    {
    GUID AbstractionLayerGuid;
    MidiEndpointDevicePurposePropertyValue EndpointPurpose;
    LPCWSTR FriendlyName;
    LPCWSTR TransportCode;
    LPCWSTR TransportSuppliedEndpointName;
    LPCWSTR TransportSuppliedEndpointDescription;
    LPCWSTR UserSuppliedEndpointName;
    LPCWSTR UserSuppliedEndpointDescription;
    UINT32 UserSuppliedEndpointPortNumber;
    LPCWSTR UniqueIdentifier;
    LPCWSTR ManufacturerName;
    MidiDataFormat SupportedDataFormats;
    BYTE NativeDataFormat;
    BOOL SupportsMidi1ProtocolDefaultValue;
    BOOL SupportsMidi2ProtocolDefaultValue;
    BOOL SupportsMultiClient;
    BOOL RequiresMetadataHandler;
    BOOL GenerateIncomingTimestamps;
    } 	MIDIENDPOINTCOMMONPROPERTIES;

typedef struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0006_0002 *PMIDIENDPOINTCOMMONPROPERTIES;



extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0006_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0006_v0_0_s_ifspec;

#ifndef __IMidiDeviceManagerInterface_INTERFACE_DEFINED__
#define __IMidiDeviceManagerInterface_INTERFACE_DEFINED__

/* interface IMidiDeviceManagerInterface */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMidiDeviceManagerInterface;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("A04374D3-4514-44E1-A2F9-7D8B907AEF1F")
    IMidiDeviceManagerInterface : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE ActivateVirtualParentDevice( 
            /* [in] */ ULONG DevPropertyCount,
            /* [annotation][in] */ 
            _In_opt_  PVOID DevProperties,
            /* [annotation][in] */ 
            _In_  PVOID CreateInfo,
            /* [annotation][out] */ 
            _Out_writes_opt_z_(CreatedSwDeviceIdCharCount)  LPWSTR CreatedSwDeviceId,
            /* [in] */ ULONG CreatedSwDeviceIdCharCount) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ActivateEndpoint( 
            /* [in] */ LPCWSTR ParentInstanceId,
            /* [in] */ BOOL UMPOnly,
            /* [in] */ MidiFlow MidiFlow,
            /* [in] */ PMIDIENDPOINTCOMMONPROPERTIES CommonProperties,
            /* [in] */ ULONG IntPropertyCount,
            /* [in] */ ULONG DevPropertyCount,
            /* [annotation][in] */ 
            _In_opt_  PVOID InterfaceDevProperties,
            /* [annotation][in] */ 
            _In_opt_  PVOID DeviceDevProperties,
            /* [annotation][in] */ 
            _In_opt_  PVOID CreateInfo,
            /* [annotation][out] */ 
            _Out_writes_opt_z_(CreatedSwDeviceInterfaceIdWCharCount)  LPWSTR CreatedSwDeviceInterfaceId,
            /* [in] */ ULONG CreatedSwDeviceInterfaceIdWCharCount) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE UpdateEndpointProperties( 
            /* [in] */ LPCWSTR DeviceInterfaceId,
            /* [in] */ ULONG IntPropertyCount,
            /* [annotation][in] */ 
            _In_  PVOID InterfaceDevProperties) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE DeleteEndpointProperties( 
            /* [in] */ LPCWSTR DeviceInterfaceId,
            /* [in] */ ULONG IntPropertyCount,
            /* [annotation][in] */ 
            _In_  PVOID DevPropKeys) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE DeactivateEndpoint( 
            /* [in] */ LPCWSTR InstanceId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE RemoveEndpoint( 
            /* [in] */ LPCWSTR InstanceId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE UpdateAbstractionConfiguration( 
            /* [in] */ GUID AbstractionId,
            /* [in] */ LPCWSTR ConfigurationJson,
            /* [in] */ BOOL IsFromConfigurationFile,
            /* [out] */ BSTR *Response) = 0;
        
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
            /* [in] */ ULONG DevPropertyCount,
            /* [annotation][in] */ 
            _In_opt_  PVOID DevProperties,
            /* [annotation][in] */ 
            _In_  PVOID CreateInfo,
            /* [annotation][out] */ 
            _Out_writes_opt_z_(CreatedSwDeviceIdCharCount)  LPWSTR CreatedSwDeviceId,
            /* [in] */ ULONG CreatedSwDeviceIdCharCount);
        
        DECLSPEC_XFGVIRT(IMidiDeviceManagerInterface, ActivateEndpoint)
        HRESULT ( STDMETHODCALLTYPE *ActivateEndpoint )( 
            IMidiDeviceManagerInterface * This,
            /* [in] */ LPCWSTR ParentInstanceId,
            /* [in] */ BOOL UMPOnly,
            /* [in] */ MidiFlow MidiFlow,
            /* [in] */ PMIDIENDPOINTCOMMONPROPERTIES CommonProperties,
            /* [in] */ ULONG IntPropertyCount,
            /* [in] */ ULONG DevPropertyCount,
            /* [annotation][in] */ 
            _In_opt_  PVOID InterfaceDevProperties,
            /* [annotation][in] */ 
            _In_opt_  PVOID DeviceDevProperties,
            /* [annotation][in] */ 
            _In_opt_  PVOID CreateInfo,
            /* [annotation][out] */ 
            _Out_writes_opt_z_(CreatedSwDeviceInterfaceIdWCharCount)  LPWSTR CreatedSwDeviceInterfaceId,
            /* [in] */ ULONG CreatedSwDeviceInterfaceIdWCharCount);
        
        DECLSPEC_XFGVIRT(IMidiDeviceManagerInterface, UpdateEndpointProperties)
        HRESULT ( STDMETHODCALLTYPE *UpdateEndpointProperties )( 
            IMidiDeviceManagerInterface * This,
            /* [in] */ LPCWSTR DeviceInterfaceId,
            /* [in] */ ULONG IntPropertyCount,
            /* [annotation][in] */ 
            _In_  PVOID InterfaceDevProperties);
        
        DECLSPEC_XFGVIRT(IMidiDeviceManagerInterface, DeleteEndpointProperties)
        HRESULT ( STDMETHODCALLTYPE *DeleteEndpointProperties )( 
            IMidiDeviceManagerInterface * This,
            /* [in] */ LPCWSTR DeviceInterfaceId,
            /* [in] */ ULONG IntPropertyCount,
            /* [annotation][in] */ 
            _In_  PVOID DevPropKeys);
        
        DECLSPEC_XFGVIRT(IMidiDeviceManagerInterface, DeactivateEndpoint)
        HRESULT ( STDMETHODCALLTYPE *DeactivateEndpoint )( 
            IMidiDeviceManagerInterface * This,
            /* [in] */ LPCWSTR InstanceId);
        
        DECLSPEC_XFGVIRT(IMidiDeviceManagerInterface, RemoveEndpoint)
        HRESULT ( STDMETHODCALLTYPE *RemoveEndpoint )( 
            IMidiDeviceManagerInterface * This,
            /* [in] */ LPCWSTR InstanceId);
        
        DECLSPEC_XFGVIRT(IMidiDeviceManagerInterface, UpdateAbstractionConfiguration)
        HRESULT ( STDMETHODCALLTYPE *UpdateAbstractionConfiguration )( 
            IMidiDeviceManagerInterface * This,
            /* [in] */ GUID AbstractionId,
            /* [in] */ LPCWSTR ConfigurationJson,
            /* [in] */ BOOL IsFromConfigurationFile,
            /* [out] */ BSTR *Response);
        
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


#define IMidiDeviceManagerInterface_ActivateVirtualParentDevice(This,DevPropertyCount,DevProperties,CreateInfo,CreatedSwDeviceId,CreatedSwDeviceIdCharCount)	\
    ( (This)->lpVtbl -> ActivateVirtualParentDevice(This,DevPropertyCount,DevProperties,CreateInfo,CreatedSwDeviceId,CreatedSwDeviceIdCharCount) ) 

#define IMidiDeviceManagerInterface_ActivateEndpoint(This,ParentInstanceId,UMPOnly,MidiFlow,CommonProperties,IntPropertyCount,DevPropertyCount,InterfaceDevProperties,DeviceDevProperties,CreateInfo,CreatedSwDeviceInterfaceId,CreatedSwDeviceInterfaceIdWCharCount)	\
    ( (This)->lpVtbl -> ActivateEndpoint(This,ParentInstanceId,UMPOnly,MidiFlow,CommonProperties,IntPropertyCount,DevPropertyCount,InterfaceDevProperties,DeviceDevProperties,CreateInfo,CreatedSwDeviceInterfaceId,CreatedSwDeviceInterfaceIdWCharCount) ) 

#define IMidiDeviceManagerInterface_UpdateEndpointProperties(This,DeviceInterfaceId,IntPropertyCount,InterfaceDevProperties)	\
    ( (This)->lpVtbl -> UpdateEndpointProperties(This,DeviceInterfaceId,IntPropertyCount,InterfaceDevProperties) ) 

#define IMidiDeviceManagerInterface_DeleteEndpointProperties(This,DeviceInterfaceId,IntPropertyCount,DevPropKeys)	\
    ( (This)->lpVtbl -> DeleteEndpointProperties(This,DeviceInterfaceId,IntPropertyCount,DevPropKeys) ) 

#define IMidiDeviceManagerInterface_DeactivateEndpoint(This,InstanceId)	\
    ( (This)->lpVtbl -> DeactivateEndpoint(This,InstanceId) ) 

#define IMidiDeviceManagerInterface_RemoveEndpoint(This,InstanceId)	\
    ( (This)->lpVtbl -> RemoveEndpoint(This,InstanceId) ) 

#define IMidiDeviceManagerInterface_UpdateAbstractionConfiguration(This,AbstractionId,ConfigurationJson,IsFromConfigurationFile,Response)	\
    ( (This)->lpVtbl -> UpdateAbstractionConfiguration(This,AbstractionId,ConfigurationJson,IsFromConfigurationFile,Response) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiDeviceManagerInterface_INTERFACE_DEFINED__ */


#ifndef __IMidiServiceConfigurationManagerInterface_INTERFACE_DEFINED__
#define __IMidiServiceConfigurationManagerInterface_INTERFACE_DEFINED__

/* interface IMidiServiceConfigurationManagerInterface */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMidiServiceConfigurationManagerInterface;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("6c21fcda-051b-4f06-8d40-f5bced5957c0")
    IMidiServiceConfigurationManagerInterface : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetAbstractionCreateActionJsonObject( 
            /* [in] */ LPCWSTR sourceAbstractionJson,
            /* [out] */ BSTR *responseJson) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetAbstractionUpdateActionJsonObject( 
            /* [in] */ LPCWSTR sourceAbstractionJson,
            /* [out] */ BSTR *responseJson) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetAbstractionRemoveActionJsonObject( 
            /* [in] */ LPCWSTR sourceAbstractionJson,
            /* [out] */ BSTR *responseJson) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetAbstractionMatchingEndpointJsonObject( 
            /* [in] */ LPCWSTR sourceActionObjectJson,
            /* [in] */ LPCWSTR searchKeyValuePairsJson,
            /* [out] */ BSTR *responseJson) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetAndPurgeConfigFileAbstractionEndpointUpdateJsonObject( 
            /* [in] */ GUID abstractionId,
            /* [in] */ LPCWSTR searchKeyValuePairsJson,
            /* [out] */ BSTR *responseJson) = 0;
        
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
        
        DECLSPEC_XFGVIRT(IMidiServiceConfigurationManagerInterface, GetAbstractionCreateActionJsonObject)
        HRESULT ( STDMETHODCALLTYPE *GetAbstractionCreateActionJsonObject )( 
            IMidiServiceConfigurationManagerInterface * This,
            /* [in] */ LPCWSTR sourceAbstractionJson,
            /* [out] */ BSTR *responseJson);
        
        DECLSPEC_XFGVIRT(IMidiServiceConfigurationManagerInterface, GetAbstractionUpdateActionJsonObject)
        HRESULT ( STDMETHODCALLTYPE *GetAbstractionUpdateActionJsonObject )( 
            IMidiServiceConfigurationManagerInterface * This,
            /* [in] */ LPCWSTR sourceAbstractionJson,
            /* [out] */ BSTR *responseJson);
        
        DECLSPEC_XFGVIRT(IMidiServiceConfigurationManagerInterface, GetAbstractionRemoveActionJsonObject)
        HRESULT ( STDMETHODCALLTYPE *GetAbstractionRemoveActionJsonObject )( 
            IMidiServiceConfigurationManagerInterface * This,
            /* [in] */ LPCWSTR sourceAbstractionJson,
            /* [out] */ BSTR *responseJson);
        
        DECLSPEC_XFGVIRT(IMidiServiceConfigurationManagerInterface, GetAbstractionMatchingEndpointJsonObject)
        HRESULT ( STDMETHODCALLTYPE *GetAbstractionMatchingEndpointJsonObject )( 
            IMidiServiceConfigurationManagerInterface * This,
            /* [in] */ LPCWSTR sourceActionObjectJson,
            /* [in] */ LPCWSTR searchKeyValuePairsJson,
            /* [out] */ BSTR *responseJson);
        
        DECLSPEC_XFGVIRT(IMidiServiceConfigurationManagerInterface, GetAndPurgeConfigFileAbstractionEndpointUpdateJsonObject)
        HRESULT ( STDMETHODCALLTYPE *GetAndPurgeConfigFileAbstractionEndpointUpdateJsonObject )( 
            IMidiServiceConfigurationManagerInterface * This,
            /* [in] */ GUID abstractionId,
            /* [in] */ LPCWSTR searchKeyValuePairsJson,
            /* [out] */ BSTR *responseJson);
        
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


#define IMidiServiceConfigurationManagerInterface_GetAbstractionCreateActionJsonObject(This,sourceAbstractionJson,responseJson)	\
    ( (This)->lpVtbl -> GetAbstractionCreateActionJsonObject(This,sourceAbstractionJson,responseJson) ) 

#define IMidiServiceConfigurationManagerInterface_GetAbstractionUpdateActionJsonObject(This,sourceAbstractionJson,responseJson)	\
    ( (This)->lpVtbl -> GetAbstractionUpdateActionJsonObject(This,sourceAbstractionJson,responseJson) ) 

#define IMidiServiceConfigurationManagerInterface_GetAbstractionRemoveActionJsonObject(This,sourceAbstractionJson,responseJson)	\
    ( (This)->lpVtbl -> GetAbstractionRemoveActionJsonObject(This,sourceAbstractionJson,responseJson) ) 

#define IMidiServiceConfigurationManagerInterface_GetAbstractionMatchingEndpointJsonObject(This,sourceActionObjectJson,searchKeyValuePairsJson,responseJson)	\
    ( (This)->lpVtbl -> GetAbstractionMatchingEndpointJsonObject(This,sourceActionObjectJson,searchKeyValuePairsJson,responseJson) ) 

#define IMidiServiceConfigurationManagerInterface_GetAndPurgeConfigFileAbstractionEndpointUpdateJsonObject(This,abstractionId,searchKeyValuePairsJson,responseJson)	\
    ( (This)->lpVtbl -> GetAndPurgeConfigFileAbstractionEndpointUpdateJsonObject(This,abstractionId,searchKeyValuePairsJson,responseJson) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiServiceConfigurationManagerInterface_INTERFACE_DEFINED__ */


#ifndef __IMidiAbstractionConfigurationManager_INTERFACE_DEFINED__
#define __IMidiAbstractionConfigurationManager_INTERFACE_DEFINED__

/* interface IMidiAbstractionConfigurationManager */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMidiAbstractionConfigurationManager;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("f19dd642-1809-4497-9eee-f230b11bd6fb")
    IMidiAbstractionConfigurationManager : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( 
            /* [in] */ GUID abstractionId,
            /* [in] */ IUnknown *midiDeviceManager,
            /* [in] */ IUnknown *midiServiceConfigurationManager) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE UpdateConfiguration( 
            /* [in] */ LPCWSTR configurationJsonSection,
            /* [in] */ BOOL IsFromConfigurationFile,
            /* [out] */ BSTR *response) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Cleanup( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiAbstractionConfigurationManagerVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiAbstractionConfigurationManager * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiAbstractionConfigurationManager * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiAbstractionConfigurationManager * This);
        
        DECLSPEC_XFGVIRT(IMidiAbstractionConfigurationManager, Initialize)
        HRESULT ( STDMETHODCALLTYPE *Initialize )( 
            IMidiAbstractionConfigurationManager * This,
            /* [in] */ GUID abstractionId,
            /* [in] */ IUnknown *midiDeviceManager,
            /* [in] */ IUnknown *midiServiceConfigurationManager);
        
        DECLSPEC_XFGVIRT(IMidiAbstractionConfigurationManager, UpdateConfiguration)
        HRESULT ( STDMETHODCALLTYPE *UpdateConfiguration )( 
            IMidiAbstractionConfigurationManager * This,
            /* [in] */ LPCWSTR configurationJsonSection,
            /* [in] */ BOOL IsFromConfigurationFile,
            /* [out] */ BSTR *response);
        
        DECLSPEC_XFGVIRT(IMidiAbstractionConfigurationManager, Cleanup)
        HRESULT ( STDMETHODCALLTYPE *Cleanup )( 
            IMidiAbstractionConfigurationManager * This);
        
        END_INTERFACE
    } IMidiAbstractionConfigurationManagerVtbl;

    interface IMidiAbstractionConfigurationManager
    {
        CONST_VTBL struct IMidiAbstractionConfigurationManagerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiAbstractionConfigurationManager_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiAbstractionConfigurationManager_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiAbstractionConfigurationManager_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiAbstractionConfigurationManager_Initialize(This,abstractionId,midiDeviceManager,midiServiceConfigurationManager)	\
    ( (This)->lpVtbl -> Initialize(This,abstractionId,midiDeviceManager,midiServiceConfigurationManager) ) 

#define IMidiAbstractionConfigurationManager_UpdateConfiguration(This,configurationJsonSection,IsFromConfigurationFile,response)	\
    ( (This)->lpVtbl -> UpdateConfiguration(This,configurationJsonSection,IsFromConfigurationFile,response) ) 

#define IMidiAbstractionConfigurationManager_Cleanup(This)	\
    ( (This)->lpVtbl -> Cleanup(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiAbstractionConfigurationManager_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_WindowsMidiServices_0000_0009 */
/* [local] */ 

typedef /* [public] */ struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0009_0001
    {
    MidiDataFormat DataFormatIn;
    MidiDataFormat DataFormatOut;
    LPCWSTR InstanceConfigurationJsonData;
    BYTE UmpGroupIndex;
    } 	TRANSFORMCREATIONPARAMS;

typedef struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0009_0001 *PTRANSFORMCREATIONPARAMS;



extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0009_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0009_v0_0_s_ifspec;

#ifndef __IMidiTransform_INTERFACE_DEFINED__
#define __IMidiTransform_INTERFACE_DEFINED__

/* interface IMidiTransform */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMidiTransform;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("65FA86A4-0433-4DCD-88E4-E565051CAB2D")
    IMidiTransform : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Activate( 
            /* [annotation][in] */ 
            _In_  REFIID Iid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **Interface) = 0;
        
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
            _In_  REFIID Iid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **Interface);
        
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


#define IMidiTransform_Activate(This,Iid,Interface)	\
    ( (This)->lpVtbl -> Activate(This,Iid,Interface) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiTransform_INTERFACE_DEFINED__ */


#ifndef __IMidiDataTransform_INTERFACE_DEFINED__
#define __IMidiDataTransform_INTERFACE_DEFINED__

/* interface IMidiDataTransform */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMidiDataTransform;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("5D2400F0-F2EE-4A51-A3BE-0AC9A19C09C4")
    IMidiDataTransform : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( 
            /* [in] */ LPCWSTR deviceId,
            /* [in] */ PTRANSFORMCREATIONPARAMS creationParams,
            /* [in] */ DWORD *mmcssTaskId,
            /* [in] */ IMidiCallback *callback,
            /* [in] */ LONGLONG context,
            /* [in] */ IUnknown *midiDeviceManager) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Cleanup( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendMidiMessage( 
            /* [in] */ PVOID message,
            /* [in] */ UINT size,
            /* [in] */ LONGLONG position) = 0;
        
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
            /* [in] */ LPCWSTR deviceId,
            /* [in] */ PTRANSFORMCREATIONPARAMS creationParams,
            /* [in] */ DWORD *mmcssTaskId,
            /* [in] */ IMidiCallback *callback,
            /* [in] */ LONGLONG context,
            /* [in] */ IUnknown *midiDeviceManager);
        
        DECLSPEC_XFGVIRT(IMidiDataTransform, Cleanup)
        HRESULT ( STDMETHODCALLTYPE *Cleanup )( 
            IMidiDataTransform * This);
        
        DECLSPEC_XFGVIRT(IMidiDataTransform, SendMidiMessage)
        HRESULT ( STDMETHODCALLTYPE *SendMidiMessage )( 
            IMidiDataTransform * This,
            /* [in] */ PVOID message,
            /* [in] */ UINT size,
            /* [in] */ LONGLONG position);
        
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


#define IMidiDataTransform_Initialize(This,deviceId,creationParams,mmcssTaskId,callback,context,midiDeviceManager)	\
    ( (This)->lpVtbl -> Initialize(This,deviceId,creationParams,mmcssTaskId,callback,context,midiDeviceManager) ) 

#define IMidiDataTransform_Cleanup(This)	\
    ( (This)->lpVtbl -> Cleanup(This) ) 

#define IMidiDataTransform_SendMidiMessage(This,message,size,position)	\
    ( (This)->lpVtbl -> SendMidiMessage(This,message,size,position) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiDataTransform_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_WindowsMidiServices_0000_0011 */
/* [local] */ 

typedef /* [public] */ struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0011_0001
    {
    BYTE Number;
    BYTE FirstGroup;
    BYTE NumberOfGroupsSpanned;
    BOOL IsActive;
    BOOL IsMIDIMessageDestination;
    BOOL IsMIDIMessageSource;
    LPCWSTR Name;
    } 	DISCOVEREDFUNCTIONBLOCK;

typedef struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0011_0001 *PDISCOVEREDFUNCTIONBLOCK;

typedef /* [public] */ struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0011_0002
    {
    BOOL AllEndpointInformationReceived;
    BOOL EndpointSupportsMIDI2Protocol;
    BOOL EndpointSupportsMIDI1Protocol;
    LPCWSTR EndpointSuppliedName;
    LPCWSTR EndpointSuppliedProductInstanceId;
    BYTE CountFunctionBlocksDeclared;
    BYTE CountFunctionBlocksReceived;
    BOOL FunctionBlocksAreStatic;
    PDISCOVEREDFUNCTIONBLOCK DiscoveredFunctionBlocks;
    } 	ENDPOINTPROTOCOLNEGOTIATIONRESULTS;

typedef struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0011_0002 *PENDPOINTPROTOCOLNEGOTIATIONRESULTS;

typedef /* [public][public] */ struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0011_0003
    {
    BOOL PreferToSendJRTimestampsToEndpoint;
    BOOL PreferToReceiveJRTimestampsFromEndpoint;
    BYTE PreferredMidiProtocol;
    WORD TimeoutMilliseconds;
    } 	ENDPOINTPROTOCOLNEGOTIATIONPARAMS;

typedef struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0011_0003 *PENDPOINTPROTOCOLNEGOTIATIONPARAMS;



extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0011_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0011_v0_0_s_ifspec;

#ifndef __IMidiProtocolNegotiationCompleteCallback_INTERFACE_DEFINED__
#define __IMidiProtocolNegotiationCompleteCallback_INTERFACE_DEFINED__

/* interface IMidiProtocolNegotiationCompleteCallback */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMidiProtocolNegotiationCompleteCallback;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("f948dc64-e03a-4e24-bc6e-437ad729cd50")
    IMidiProtocolNegotiationCompleteCallback : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE ProtocolNegotiationCompleteCallback( 
            /* [in] */ GUID AbstractionGuid,
            /* [in] */ LPCWSTR DeviceInterfaceId,
            /* [in] */ PENDPOINTPROTOCOLNEGOTIATIONRESULTS Results) = 0;
        
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
            /* [in] */ GUID AbstractionGuid,
            /* [in] */ LPCWSTR DeviceInterfaceId,
            /* [in] */ PENDPOINTPROTOCOLNEGOTIATIONRESULTS Results);
        
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


#define IMidiProtocolNegotiationCompleteCallback_ProtocolNegotiationCompleteCallback(This,AbstractionGuid,DeviceInterfaceId,Results)	\
    ( (This)->lpVtbl -> ProtocolNegotiationCompleteCallback(This,AbstractionGuid,DeviceInterfaceId,Results) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiProtocolNegotiationCompleteCallback_INTERFACE_DEFINED__ */


#ifndef __IMidiEndpointProtocolManagerInterface_INTERFACE_DEFINED__
#define __IMidiEndpointProtocolManagerInterface_INTERFACE_DEFINED__

/* interface IMidiEndpointProtocolManagerInterface */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMidiEndpointProtocolManagerInterface;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("7a3def20-fb76-49b6-a3a0-3dcbcda8f482")
    IMidiEndpointProtocolManagerInterface : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE DiscoverAndNegotiate( 
            /* [in] */ GUID AbstractionGuid,
            /* [in] */ LPCWSTR DeviceInterfaceId,
            /* [in] */ ENDPOINTPROTOCOLNEGOTIATIONPARAMS NegotiationParams,
            /* [in] */ IMidiProtocolNegotiationCompleteCallback *NegotiationCompleteCallback) = 0;
        
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
            /* [in] */ GUID AbstractionGuid,
            /* [in] */ LPCWSTR DeviceInterfaceId,
            /* [in] */ ENDPOINTPROTOCOLNEGOTIATIONPARAMS NegotiationParams,
            /* [in] */ IMidiProtocolNegotiationCompleteCallback *NegotiationCompleteCallback);
        
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


#define IMidiEndpointProtocolManagerInterface_DiscoverAndNegotiate(This,AbstractionGuid,DeviceInterfaceId,NegotiationParams,NegotiationCompleteCallback)	\
    ( (This)->lpVtbl -> DiscoverAndNegotiate(This,AbstractionGuid,DeviceInterfaceId,NegotiationParams,NegotiationCompleteCallback) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiEndpointProtocolManagerInterface_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_WindowsMidiServices_0000_0013 */
/* [local] */ 

typedef /* [public] */ struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0013_0001
    {
    GUID Id;
    LPCWSTR TransportCode;
    BSTR Name;
    BSTR Description;
    BSTR Author;
    BSTR SmallImagePath;
    BSTR Version;
    BOOL IsRuntimeCreatableByApps;
    BOOL IsRuntimeCreatableBySettings;
    BOOL IsSystemManaged;
    BOOL IsClientConfigurable;
    } 	ABSTRACTIONMETADATA;

typedef struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0013_0001 *PABSTRACTIONMETADATA;

typedef /* [public] */ struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0013_0002
    {
    GUID Id;
    BSTR Name;
    BSTR Description;
    BSTR Author;
    BSTR SmallImagePath;
    BSTR Version;
    BOOL IsRuntimeCreatableByApps;
    BOOL IsRuntimeCreatableBySettings;
    BOOL IsSystemManaged;
    BOOL IsClientConfigurable;
    } 	TRANSFORMMETADATA;

typedef struct __MIDL___MIDL_itf_WindowsMidiServices_0000_0013_0002 *PTRANSFORMMETADATA;



extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0013_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_WindowsMidiServices_0000_0013_v0_0_s_ifspec;

#ifndef __IMidiServiceAbstractionPluginMetadataProvider_INTERFACE_DEFINED__
#define __IMidiServiceAbstractionPluginMetadataProvider_INTERFACE_DEFINED__

/* interface IMidiServiceAbstractionPluginMetadataProvider */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMidiServiceAbstractionPluginMetadataProvider;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8d542740-83f8-4a9a-8627-cd6324b6c1f4")
    IMidiServiceAbstractionPluginMetadataProvider : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMetadata( 
            /* [out] */ PABSTRACTIONMETADATA metadata) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Cleanup( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMidiServiceAbstractionPluginMetadataProviderVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMidiServiceAbstractionPluginMetadataProvider * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMidiServiceAbstractionPluginMetadataProvider * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMidiServiceAbstractionPluginMetadataProvider * This);
        
        DECLSPEC_XFGVIRT(IMidiServiceAbstractionPluginMetadataProvider, Initialize)
        HRESULT ( STDMETHODCALLTYPE *Initialize )( 
            IMidiServiceAbstractionPluginMetadataProvider * This);
        
        DECLSPEC_XFGVIRT(IMidiServiceAbstractionPluginMetadataProvider, GetMetadata)
        HRESULT ( STDMETHODCALLTYPE *GetMetadata )( 
            IMidiServiceAbstractionPluginMetadataProvider * This,
            /* [out] */ PABSTRACTIONMETADATA metadata);
        
        DECLSPEC_XFGVIRT(IMidiServiceAbstractionPluginMetadataProvider, Cleanup)
        HRESULT ( STDMETHODCALLTYPE *Cleanup )( 
            IMidiServiceAbstractionPluginMetadataProvider * This);
        
        END_INTERFACE
    } IMidiServiceAbstractionPluginMetadataProviderVtbl;

    interface IMidiServiceAbstractionPluginMetadataProvider
    {
        CONST_VTBL struct IMidiServiceAbstractionPluginMetadataProviderVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMidiServiceAbstractionPluginMetadataProvider_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMidiServiceAbstractionPluginMetadataProvider_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMidiServiceAbstractionPluginMetadataProvider_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMidiServiceAbstractionPluginMetadataProvider_Initialize(This)	\
    ( (This)->lpVtbl -> Initialize(This) ) 

#define IMidiServiceAbstractionPluginMetadataProvider_GetMetadata(This,metadata)	\
    ( (This)->lpVtbl -> GetMetadata(This,metadata) ) 

#define IMidiServiceAbstractionPluginMetadataProvider_Cleanup(This)	\
    ( (This)->lpVtbl -> Cleanup(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiServiceAbstractionPluginMetadataProvider_INTERFACE_DEFINED__ */


#ifndef __IMidiServiceTransformPluginMetadataProvider_INTERFACE_DEFINED__
#define __IMidiServiceTransformPluginMetadataProvider_INTERFACE_DEFINED__

/* interface IMidiServiceTransformPluginMetadataProvider */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMidiServiceTransformPluginMetadataProvider;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("76c65f88-061f-4e4f-bb9d-3ac1981f12f2")
    IMidiServiceTransformPluginMetadataProvider : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMetadata( 
            /* [out] */ PTRANSFORMMETADATA metadata) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Cleanup( void) = 0;
        
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
            /* [out] */ PTRANSFORMMETADATA metadata);
        
        DECLSPEC_XFGVIRT(IMidiServiceTransformPluginMetadataProvider, Cleanup)
        HRESULT ( STDMETHODCALLTYPE *Cleanup )( 
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

#define IMidiServiceTransformPluginMetadataProvider_Cleanup(This)	\
    ( (This)->lpVtbl -> Cleanup(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiServiceTransformPluginMetadataProvider_INTERFACE_DEFINED__ */


#ifndef __IMidiServicePluginMetadataReporterInterface_INTERFACE_DEFINED__
#define __IMidiServicePluginMetadataReporterInterface_INTERFACE_DEFINED__

/* interface IMidiServicePluginMetadataReporterInterface */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMidiServicePluginMetadataReporterInterface;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("469c7722-f779-40c3-b648-52620f75dcee")
    IMidiServicePluginMetadataReporterInterface : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetAbstractionList( 
            /* [out] */ BSTR *AbstractionListJson) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetTransformList( 
            /* [out] */ BSTR *TransformListJson) = 0;
        
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
        
        DECLSPEC_XFGVIRT(IMidiServicePluginMetadataReporterInterface, GetAbstractionList)
        HRESULT ( STDMETHODCALLTYPE *GetAbstractionList )( 
            IMidiServicePluginMetadataReporterInterface * This,
            /* [out] */ BSTR *AbstractionListJson);
        
        DECLSPEC_XFGVIRT(IMidiServicePluginMetadataReporterInterface, GetTransformList)
        HRESULT ( STDMETHODCALLTYPE *GetTransformList )( 
            IMidiServicePluginMetadataReporterInterface * This,
            /* [out] */ BSTR *TransformListJson);
        
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


#define IMidiServicePluginMetadataReporterInterface_GetAbstractionList(This,AbstractionListJson)	\
    ( (This)->lpVtbl -> GetAbstractionList(This,AbstractionListJson) ) 

#define IMidiServicePluginMetadataReporterInterface_GetTransformList(This,TransformListJson)	\
    ( (This)->lpVtbl -> GetTransformList(This,TransformListJson) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiServicePluginMetadataReporterInterface_INTERFACE_DEFINED__ */


#ifndef __IMidiSessionTracker_INTERFACE_DEFINED__
#define __IMidiSessionTracker_INTERFACE_DEFINED__

/* interface IMidiSessionTracker */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMidiSessionTracker;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("194c2746-3ae5-419a-94d9-20416c7dbefe")
    IMidiSessionTracker : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Initialize( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE AddClientSession( 
            /* [in] */ GUID SessionId,
            /* [in] */ LPCWSTR SessionName) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE UpdateClientSessionName( 
            /* [in] */ GUID SessionId,
            /* [in] */ LPCWSTR SessionName) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE RemoveClientSession( 
            /* [in] */ GUID SessionId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetSessionList( 
            /* [out] */ BSTR *SessionDetailsList) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE VerifyConnectivity( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Cleanup( void) = 0;
        
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
            /* [in] */ GUID SessionId,
            /* [in] */ LPCWSTR SessionName);
        
        DECLSPEC_XFGVIRT(IMidiSessionTracker, UpdateClientSessionName)
        HRESULT ( STDMETHODCALLTYPE *UpdateClientSessionName )( 
            IMidiSessionTracker * This,
            /* [in] */ GUID SessionId,
            /* [in] */ LPCWSTR SessionName);
        
        DECLSPEC_XFGVIRT(IMidiSessionTracker, RemoveClientSession)
        HRESULT ( STDMETHODCALLTYPE *RemoveClientSession )( 
            IMidiSessionTracker * This,
            /* [in] */ GUID SessionId);
        
        DECLSPEC_XFGVIRT(IMidiSessionTracker, GetSessionList)
        HRESULT ( STDMETHODCALLTYPE *GetSessionList )( 
            IMidiSessionTracker * This,
            /* [out] */ BSTR *SessionDetailsList);
        
        DECLSPEC_XFGVIRT(IMidiSessionTracker, VerifyConnectivity)
        HRESULT ( STDMETHODCALLTYPE *VerifyConnectivity )( 
            IMidiSessionTracker * This);
        
        DECLSPEC_XFGVIRT(IMidiSessionTracker, Cleanup)
        HRESULT ( STDMETHODCALLTYPE *Cleanup )( 
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

#define IMidiSessionTracker_AddClientSession(This,SessionId,SessionName)	\
    ( (This)->lpVtbl -> AddClientSession(This,SessionId,SessionName) ) 

#define IMidiSessionTracker_UpdateClientSessionName(This,SessionId,SessionName)	\
    ( (This)->lpVtbl -> UpdateClientSessionName(This,SessionId,SessionName) ) 

#define IMidiSessionTracker_RemoveClientSession(This,SessionId)	\
    ( (This)->lpVtbl -> RemoveClientSession(This,SessionId) ) 

#define IMidiSessionTracker_GetSessionList(This,SessionDetailsList)	\
    ( (This)->lpVtbl -> GetSessionList(This,SessionDetailsList) ) 

#define IMidiSessionTracker_VerifyConnectivity(This)	\
    ( (This)->lpVtbl -> VerifyConnectivity(This) ) 

#define IMidiSessionTracker_Cleanup(This)	\
    ( (This)->lpVtbl -> Cleanup(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiSessionTracker_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


