

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for C:\Users\Pete\AppData\Local\Temp\MidiUniversalSystemExclusiveChannel.idl-15de2457:
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

#ifndef __MidiUniversalSystemExclusiveChannel_h_p_h__
#define __MidiUniversalSystemExclusiveChannel_h_p_h__

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

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel_FWD_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics_FWD_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory_FWD_DEFINED__ */


/* header files for imported files */
#include "Windows.Foundation.h"
#include "MidiApiContracts.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_MidiUniversalSystemExclusiveChannel_0000_0000 */
/* [local] */ 





#pragma once 



extern RPC_IF_HANDLE __MIDL_itf_MidiUniversalSystemExclusiveChannel_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiUniversalSystemExclusiveChannel_0000_0000_v0_0_s_ifspec;

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-dd00e0107000")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel : public IInspectable
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

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel, get_Index)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Index )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel * This,
            /* [retval][out] */ byte *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel, put_Index)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_Index )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel * This,
            /* [in] */ byte value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel, get_DisplayValue)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_DisplayValue )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel * This,
            /* [retval][out] */ byte *value);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel_get_Index(This,value)	\
    ( (This)->lpVtbl -> get_Index(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel_put_Index(This,value)	\
    ( (This)->lpVtbl -> put_Index(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel_get_DisplayValue(This,value)	\
    ( (This)->lpVtbl -> get_DisplayValue(This,value) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel_INTERFACE_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-ee00e0107000")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics : public IInspectable
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
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_DisregardChannel( 
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel **value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE IsValidIndex( 
            /* [in] */ byte index,
            /* [retval][out] */ boolean *result) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStaticsVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics, get_ShortLabel)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_ShortLabel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics * This,
            /* [retval][out] */ HSTRING *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics, get_ShortLabelPlural)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_ShortLabelPlural )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics * This,
            /* [retval][out] */ HSTRING *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics, get_LongLabel)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_LongLabel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics * This,
            /* [retval][out] */ HSTRING *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics, get_LongLabelPlural)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_LongLabelPlural )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics * This,
            /* [retval][out] */ HSTRING *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics, get_DisregardChannel)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_DisregardChannel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics * This,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel **value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics, IsValidIndex)
        HRESULT ( STDMETHODCALLTYPE *IsValidIndex )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics * This,
            /* [in] */ byte index,
            /* [retval][out] */ boolean *result);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStaticsVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStaticsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics_get_ShortLabel(This,value)	\
    ( (This)->lpVtbl -> get_ShortLabel(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics_get_ShortLabelPlural(This,value)	\
    ( (This)->lpVtbl -> get_ShortLabelPlural(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics_get_LongLabel(This,value)	\
    ( (This)->lpVtbl -> get_LongLabel(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics_get_LongLabelPlural(This,value)	\
    ( (This)->lpVtbl -> get_LongLabelPlural(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics_get_DisregardChannel(This,value)	\
    ( (This)->lpVtbl -> get_DisregardChannel(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics_IsValidIndex(This,index,result)	\
    ( (This)->lpVtbl -> IsValidIndex(This,index,result) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelStatics_INTERFACE_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("abc2073f-2a63-54ae-ad89-413abbf70d8f")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory : public IInspectable
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE CreateInstance( 
            /* [in] */ byte index,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel **value) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactoryVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory, CreateInstance)
        HRESULT ( STDMETHODCALLTYPE *CreateInstance )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory * This,
            /* [in] */ byte index,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannel **value);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactoryVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactoryVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory_CreateInstance(This,index,value)	\
    ( (This)->lpVtbl -> CreateInstance(This,index,value) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiUniversalSystemExclusiveChannelFactory_INTERFACE_DEFINED__ */


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


