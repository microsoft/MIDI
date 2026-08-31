

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for C:\Users\Pete\AppData\Local\Temp\MidiMessage128.idl-070f9d47:
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

#ifndef __MidiMessage128_h_p_h__
#define __MidiMessage128_h_p_h__

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

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_FWD_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics_FWD_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory_FWD_DEFINED__ */


/* header files for imported files */
#include "Windows.Foundation.h"
#include "MidiApiContracts.h"
#include "IMidiUniversalPacket.h"
#include "MidiMessageStruct.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_MidiMessage128_0000_0000 */
/* [local] */ 





#pragma once 



extern RPC_IF_HANDLE __MIDL_itf_MidiMessage128_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiMessage128_0000_0000_v0_0_s_ifspec;

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-dd0010004128")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 : public IInspectable
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Word0( 
            /* [retval][out] */ unsigned int *value) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_Word0( 
            /* [in] */ unsigned int value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Word1( 
            /* [retval][out] */ unsigned int *value) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_Word1( 
            /* [in] */ unsigned int value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Word2( 
            /* [retval][out] */ unsigned int *value) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_Word2( 
            /* [in] */ unsigned int value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Word3( 
            /* [retval][out] */ unsigned int *value) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_Word3( 
            /* [in] */ unsigned int value) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Vtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128, get_Word0)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Word0 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 * This,
            /* [retval][out] */ unsigned int *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128, put_Word0)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_Word0 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 * This,
            /* [in] */ unsigned int value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128, get_Word1)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Word1 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 * This,
            /* [retval][out] */ unsigned int *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128, put_Word1)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_Word1 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 * This,
            /* [in] */ unsigned int value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128, get_Word2)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Word2 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 * This,
            /* [retval][out] */ unsigned int *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128, put_Word2)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_Word2 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 * This,
            /* [in] */ unsigned int value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128, get_Word3)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Word3 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 * This,
            /* [retval][out] */ unsigned int *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128, put_Word3)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_Word3 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 * This,
            /* [in] */ unsigned int value);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Vtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_get_Word0(This,value)	\
    ( (This)->lpVtbl -> get_Word0(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_put_Word0(This,value)	\
    ( (This)->lpVtbl -> put_Word0(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_get_Word1(This,value)	\
    ( (This)->lpVtbl -> get_Word1(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_put_Word1(This,value)	\
    ( (This)->lpVtbl -> put_Word1(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_get_Word2(This,value)	\
    ( (This)->lpVtbl -> get_Word2(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_put_Word2(This,value)	\
    ( (This)->lpVtbl -> put_Word2(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_get_Word3(This,value)	\
    ( (This)->lpVtbl -> get_Word3(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_put_Word3(This,value)	\
    ( (This)->lpVtbl -> put_Word3(This,value) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128_INTERFACE_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-ee0010004128")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics : public IInspectable
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE CreateFromStruct( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ struct __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageStruct message,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 **result) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128StaticsVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics, CreateFromStruct)
        HRESULT ( STDMETHODCALLTYPE *CreateFromStruct )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ struct __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageStruct message,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 **result);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128StaticsVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128StaticsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics_CreateFromStruct(This,timestamp,message,result)	\
    ( (This)->lpVtbl -> CreateFromStruct(This,timestamp,message,result) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Statics_INTERFACE_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("6d284202-a372-5f12-a707-3c4916e123d0")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory : public IInspectable
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE CreateInstance( 
            IInspectable *baseInterface,
            /* [out] */ IInspectable **innerInterface,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 **value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE CreateInstance2( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int word0,
            /* [in] */ unsigned int word1,
            /* [in] */ unsigned int word2,
            /* [in] */ unsigned int word3,
            IInspectable *baseInterface,
            /* [out] */ IInspectable **innerInterface,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 **value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE CreateInstance3( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int wordsLength,
            /* [in][size_is] */ unsigned int *words,
            IInspectable *baseInterface,
            /* [out] */ IInspectable **innerInterface,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 **value) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128FactoryVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory, CreateInstance)
        HRESULT ( STDMETHODCALLTYPE *CreateInstance )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory * This,
            IInspectable *baseInterface,
            /* [out] */ IInspectable **innerInterface,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 **value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory, CreateInstance2)
        HRESULT ( STDMETHODCALLTYPE *CreateInstance2 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int word0,
            /* [in] */ unsigned int word1,
            /* [in] */ unsigned int word2,
            /* [in] */ unsigned int word3,
            IInspectable *baseInterface,
            /* [out] */ IInspectable **innerInterface,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 **value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory, CreateInstance3)
        HRESULT ( STDMETHODCALLTYPE *CreateInstance3 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int wordsLength,
            /* [in][size_is] */ unsigned int *words,
            IInspectable *baseInterface,
            /* [out] */ IInspectable **innerInterface,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128 **value);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128FactoryVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128FactoryVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory_CreateInstance(This,baseInterface,innerInterface,value)	\
    ( (This)->lpVtbl -> CreateInstance(This,baseInterface,innerInterface,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory_CreateInstance2(This,timestamp,word0,word1,word2,word3,baseInterface,innerInterface,value)	\
    ( (This)->lpVtbl -> CreateInstance2(This,timestamp,word0,word1,word2,word3,baseInterface,innerInterface,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory_CreateInstance3(This,timestamp,wordsLength,words,baseInterface,innerInterface,value)	\
    ( (This)->lpVtbl -> CreateInstance3(This,timestamp,wordsLength,words,baseInterface,innerInterface,value) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage128Factory_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


