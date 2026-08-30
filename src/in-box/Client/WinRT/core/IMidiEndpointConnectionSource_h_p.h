

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for C:\Users\Pete\AppData\Local\Temp\IMidiEndpointConnectionSource.idl-271e637f:
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

#ifndef __IMidiEndpointConnectionSource_h_p_h__
#define __IMidiEndpointConnectionSource_h_p_h__

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

#ifndef ____FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable_FWD_DEFINED__
#define ____FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable_FWD_DEFINED__
typedef interface __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable;

#endif 	/* ____FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable_FWD_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_FWD_DEFINED__ */


/* header files for imported files */
#include "Windows.Foundation.h"
#include "MidiApiContracts.h"
#include "IMidiEndpointConnectionSettings.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_IMidiEndpointConnectionSource_0000_0000 */
/* [local] */ 






#pragma once 


extern RPC_IF_HANDLE __MIDL_itf_IMidiEndpointConnectionSource_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_IMidiEndpointConnectionSource_0000_0000_v0_0_s_ifspec;

/* interface __MIDL_itf_IMidiEndpointConnectionSource2Eidl_0000_0194 */




extern RPC_IF_HANDLE __MIDL_itf_IMidiEndpointConnectionSource2Eidl_0000_0194_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_IMidiEndpointConnectionSource2Eidl_0000_0194_v0_0_s_ifspec;

/* interface __MIDL_itf_IMidiEndpointConnectionSource_0000_0001 */
/* [local] */ 

#ifndef DEF___FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable
#define DEF___FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable
#if !defined(__cplusplus) || defined(RO_NO_TEMPLATE_NAME)


extern RPC_IF_HANDLE __MIDL_itf_IMidiEndpointConnectionSource_0000_0001_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_IMidiEndpointConnectionSource_0000_0001_v0_0_s_ifspec;

#ifndef ____FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable_INTERFACE_DEFINED__
#define ____FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable_INTERFACE_DEFINED__

/* interface __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID___FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("d4742943-33c2-5eae-a708-c1918f54423f")
    __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Invoke( 
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource *sender,
            /* [in] */ IInspectable *e) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectableVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable * This);
        
        DECLSPEC_XFGVIRT(__FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable, Invoke)
        HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable * This,
            /* [in] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource *sender,
            /* [in] */ IInspectable *e);
        
        END_INTERFACE
    } __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectableVtbl;

    interface __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable
    {
        CONST_VTBL struct __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectableVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable_Invoke(This,sender,e)	\
    ( (This)->lpVtbl -> Invoke(This,sender,e) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_IMidiEndpointConnectionSource_0000_0002 */
/* [local] */ 

#endif /* pinterface */
#endif /* DEF___FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable */


extern RPC_IF_HANDLE __MIDL_itf_IMidiEndpointConnectionSource_0000_0002_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_IMidiEndpointConnectionSource_0000_0002_v0_0_s_ifspec;

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource */
/* [uuid][object] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-ff001000F030")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource : public IInspectable
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE add_EndpointDeviceDisconnected( 
            /* [in] */ __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable *handler,
            /* [out][retval] */ EventRegistrationToken *token) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE remove_EndpointDeviceDisconnected( 
            /* [in] */ EventRegistrationToken token) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE add_EndpointDeviceReconnected( 
            /* [in] */ __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable *handler,
            /* [out][retval] */ EventRegistrationToken *token) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE remove_EndpointDeviceReconnected( 
            /* [in] */ EventRegistrationToken token) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Tag( 
            /* [retval][out] */ IInspectable **value) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_Tag( 
            /* [in] */ IInspectable *value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_ConnectionId( 
            /* [retval][out] */ GUID *value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_ConnectedEndpointDeviceId( 
            /* [retval][out] */ HSTRING *value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Settings( 
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSettings **value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_IsOpen( 
            /* [retval][out] */ boolean *value) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSourceVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource, add_EndpointDeviceDisconnected)
        HRESULT ( STDMETHODCALLTYPE *add_EndpointDeviceDisconnected )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource * This,
            /* [in] */ __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable *handler,
            /* [out][retval] */ EventRegistrationToken *token);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource, remove_EndpointDeviceDisconnected)
        HRESULT ( STDMETHODCALLTYPE *remove_EndpointDeviceDisconnected )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource * This,
            /* [in] */ EventRegistrationToken token);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource, add_EndpointDeviceReconnected)
        HRESULT ( STDMETHODCALLTYPE *add_EndpointDeviceReconnected )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource * This,
            /* [in] */ __FITypedEventHandler_2_Windows__CDevices__CMidi2__CIMidiEndpointConnectionSource_IInspectable *handler,
            /* [out][retval] */ EventRegistrationToken *token);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource, remove_EndpointDeviceReconnected)
        HRESULT ( STDMETHODCALLTYPE *remove_EndpointDeviceReconnected )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource * This,
            /* [in] */ EventRegistrationToken token);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource, get_Tag)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Tag )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource * This,
            /* [retval][out] */ IInspectable **value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource, put_Tag)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_Tag )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource * This,
            /* [in] */ IInspectable *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource, get_ConnectionId)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_ConnectionId )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource * This,
            /* [retval][out] */ GUID *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource, get_ConnectedEndpointDeviceId)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_ConnectedEndpointDeviceId )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource * This,
            /* [retval][out] */ HSTRING *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource, get_Settings)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Settings )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource * This,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSettings **value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource, get_IsOpen)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_IsOpen )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource * This,
            /* [retval][out] */ boolean *value);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSourceVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSourceVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_add_EndpointDeviceDisconnected(This,handler,token)	\
    ( (This)->lpVtbl -> add_EndpointDeviceDisconnected(This,handler,token) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_remove_EndpointDeviceDisconnected(This,token)	\
    ( (This)->lpVtbl -> remove_EndpointDeviceDisconnected(This,token) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_add_EndpointDeviceReconnected(This,handler,token)	\
    ( (This)->lpVtbl -> add_EndpointDeviceReconnected(This,handler,token) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_remove_EndpointDeviceReconnected(This,token)	\
    ( (This)->lpVtbl -> remove_EndpointDeviceReconnected(This,token) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_get_Tag(This,value)	\
    ( (This)->lpVtbl -> get_Tag(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_put_Tag(This,value)	\
    ( (This)->lpVtbl -> put_Tag(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_get_ConnectionId(This,value)	\
    ( (This)->lpVtbl -> get_ConnectionId(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_get_ConnectedEndpointDeviceId(This,value)	\
    ( (This)->lpVtbl -> get_ConnectedEndpointDeviceId(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_get_Settings(This,value)	\
    ( (This)->lpVtbl -> get_Settings(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_get_IsOpen(This,value)	\
    ( (This)->lpVtbl -> get_IsOpen(This,value) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiEndpointConnectionSource_INTERFACE_DEFINED__ */


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


