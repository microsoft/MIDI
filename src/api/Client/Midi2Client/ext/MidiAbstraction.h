

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for MidiAbstraction.idl:
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

#ifndef __MidiAbstraction_h__
#define __MidiAbstraction_h__

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

#ifndef __IMidiCallback_FWD_DEFINED__
#define __IMidiCallback_FWD_DEFINED__
typedef interface IMidiCallback IMidiCallback;

#endif 	/* __IMidiCallback_FWD_DEFINED__ */


#ifndef __IMidiAbstraction_FWD_DEFINED__
#define __IMidiAbstraction_FWD_DEFINED__
typedef interface IMidiAbstraction IMidiAbstraction;

#endif 	/* __IMidiAbstraction_FWD_DEFINED__ */


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


/* header files for imported files */
#include "unknwn.h"

#ifdef __cplusplus
extern "C"{
#endif 


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
            /* [in] */ LONGLONG position) = 0;
        
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
            /* [in] */ LONGLONG position);
        
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


#define IMidiCallback_Callback(This,message,size,position)	\
    ( (This)->lpVtbl -> Callback(This,message,size,position) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiCallback_INTERFACE_DEFINED__ */


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
            /* [in] */ DWORD *mmcssTaskId,
            /* [in] */ IMidiCallback *callback) = 0;
        
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
            /* [in] */ DWORD *mmcssTaskId,
            /* [in] */ IMidiCallback *callback);
        
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


#define IMidiIn_Initialize(This,deviceId,mmcssTaskId,callback)	\
    ( (This)->lpVtbl -> Initialize(This,deviceId,mmcssTaskId,callback) ) 

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
            /* [in] */ DWORD *mmcssTaskId) = 0;
        
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
            /* [in] */ DWORD *mmcssTaskId);
        
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


#define IMidiOut_Initialize(This,deviceId,mmcssTaskId)	\
    ( (This)->lpVtbl -> Initialize(This,deviceId,mmcssTaskId) ) 

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
            /* [in] */ DWORD *mmcssTaskId,
            /* [in] */ IMidiCallback *callback) = 0;
        
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
            /* [in] */ DWORD *mmcssTaskId,
            /* [in] */ IMidiCallback *callback);
        
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


#define IMidiBiDi_Initialize(This,deviceId,mmcssTaskId,callback)	\
    ( (This)->lpVtbl -> Initialize(This,deviceId,mmcssTaskId,callback) ) 

#define IMidiBiDi_Cleanup(This)	\
    ( (This)->lpVtbl -> Cleanup(This) ) 

#define IMidiBiDi_SendMidiMessage(This,message,size,position)	\
    ( (This)->lpVtbl -> SendMidiMessage(This,message,size,position) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMidiBiDi_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


