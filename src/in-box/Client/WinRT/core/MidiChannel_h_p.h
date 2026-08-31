

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for C:\Users\Pete\AppData\Local\Temp\MidiChannel.idl-6047d719:
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

#ifndef __MidiChannel_h_p_h__
#define __MidiChannel_h_p_h__

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

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel_FWD_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics_FWD_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory_FWD_DEFINED__ */


/* header files for imported files */
#include "Windows.Foundation.h"
#include "MidiApiContracts.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_MidiChannel_0000_0000 */
/* [local] */ 





#pragma once 



extern RPC_IF_HANDLE __MIDL_itf_MidiChannel_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiChannel_0000_0000_v0_0_s_ifspec;

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-dd0010001000")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel : public IInspectable
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Index( 
            /* [retval][out] */ byte *value) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_Index( 
            /* [in] */ byte value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_DisplayValue( 
            /* [retval][out] */ byte *value) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel, get_Index)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Index )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel * This,
            /* [retval][out] */ byte *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel, put_Index)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_Index )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel * This,
            /* [in] */ byte value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel, get_DisplayValue)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_DisplayValue )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel * This,
            /* [retval][out] */ byte *value);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel_get_Index(This,value)	\
    ( (This)->lpVtbl -> get_Index(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel_put_Index(This,value)	\
    ( (This)->lpVtbl -> put_Index(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel_get_DisplayValue(This,value)	\
    ( (This)->lpVtbl -> get_DisplayValue(This,value) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel_INTERFACE_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-ee0010001000")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics : public IInspectable
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_ShortLabel( 
            /* [retval][out] */ HSTRING *value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_ShortLabelPlural( 
            /* [retval][out] */ HSTRING *value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_LongLabel( 
            /* [retval][out] */ HSTRING *value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_LongLabelPlural( 
            /* [retval][out] */ HSTRING *value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE IsValidIndex( 
            /* [in] */ byte index,
            /* [retval][out] */ boolean *result) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStaticsVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics, get_ShortLabel)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_ShortLabel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics * This,
            /* [retval][out] */ HSTRING *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics, get_ShortLabelPlural)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_ShortLabelPlural )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics * This,
            /* [retval][out] */ HSTRING *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics, get_LongLabel)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_LongLabel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics * This,
            /* [retval][out] */ HSTRING *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics, get_LongLabelPlural)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_LongLabelPlural )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics * This,
            /* [retval][out] */ HSTRING *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics, IsValidIndex)
        HRESULT ( STDMETHODCALLTYPE *IsValidIndex )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics * This,
            /* [in] */ byte index,
            /* [retval][out] */ boolean *result);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStaticsVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStaticsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics_get_ShortLabel(This,value)	\
    ( (This)->lpVtbl -> get_ShortLabel(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics_get_ShortLabelPlural(This,value)	\
    ( (This)->lpVtbl -> get_ShortLabelPlural(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics_get_LongLabel(This,value)	\
    ( (This)->lpVtbl -> get_LongLabel(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics_get_LongLabelPlural(This,value)	\
    ( (This)->lpVtbl -> get_LongLabelPlural(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics_IsValidIndex(This,index,result)	\
    ( (This)->lpVtbl -> IsValidIndex(This,index,result) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelStatics_INTERFACE_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("a8dfa5f4-7712-50b7-ac70-be6f9bafc0c4")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory : public IInspectable
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE CreateInstance( 
            /* [in] */ byte index,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel **value) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactoryVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory, CreateInstance)
        HRESULT ( STDMETHODCALLTYPE *CreateInstance )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory * This,
            /* [in] */ byte index,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannel **value);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactoryVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactoryVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory_CreateInstance(This,index,value)	\
    ( (This)->lpVtbl -> CreateInstance(This,index,value) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiChannelFactory_INTERFACE_DEFINED__ */


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


