

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for C:\Users\Pete\AppData\Local\Temp\MidiMessage96.idl-6ba53a4f:
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

#ifndef __MidiMessage96_h_p_h__
#define __MidiMessage96_h_p_h__

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

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96_FWD_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics_FWD_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory_FWD_DEFINED__ */


/* header files for imported files */
#include "Windows.Foundation.h"
#include "MidiApiContracts.h"
#include "IMidiUniversalPacket.h"
#include "MidiMessageStruct.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_MidiMessage96_0000_0000 */
/* [local] */ 





#pragma once 



extern RPC_IF_HANDLE __MIDL_itf_MidiMessage96_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiMessage96_0000_0000_v0_0_s_ifspec;

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-dd0010004096")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 : public IInspectable
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
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Vtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96, get_Word0)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Word0 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 * This,
            /* [retval][out] */ unsigned int *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96, put_Word0)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_Word0 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 * This,
            /* [in] */ unsigned int value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96, get_Word1)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Word1 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 * This,
            /* [retval][out] */ unsigned int *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96, put_Word1)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_Word1 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 * This,
            /* [in] */ unsigned int value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96, get_Word2)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Word2 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 * This,
            /* [retval][out] */ unsigned int *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96, put_Word2)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_Word2 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 * This,
            /* [in] */ unsigned int value);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Vtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96_get_Word0(This,value)	\
    ( (This)->lpVtbl -> get_Word0(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96_put_Word0(This,value)	\
    ( (This)->lpVtbl -> put_Word0(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96_get_Word1(This,value)	\
    ( (This)->lpVtbl -> get_Word1(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96_put_Word1(This,value)	\
    ( (This)->lpVtbl -> put_Word1(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96_get_Word2(This,value)	\
    ( (This)->lpVtbl -> get_Word2(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96_put_Word2(This,value)	\
    ( (This)->lpVtbl -> put_Word2(This,value) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96_INTERFACE_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-ee0010004096")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics : public IInspectable
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE CreateFromStruct( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ struct __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageStruct message,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 **result) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96StaticsVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics, CreateFromStruct)
        HRESULT ( STDMETHODCALLTYPE *CreateFromStruct )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ struct __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageStruct message,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 **result);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96StaticsVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96StaticsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics_CreateFromStruct(This,timestamp,message,result)	\
    ( (This)->lpVtbl -> CreateFromStruct(This,timestamp,message,result) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Statics_INTERFACE_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("38d1c730-7f53-557a-848e-0f46fef4e1e5")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory : public IInspectable
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE CreateInstance( 
            IInspectable *baseInterface,
            /* [out] */ IInspectable **innerInterface,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 **value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE CreateInstance2( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int word0,
            /* [in] */ unsigned int word1,
            /* [in] */ unsigned int word2,
            IInspectable *baseInterface,
            /* [out] */ IInspectable **innerInterface,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 **value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE CreateInstance3( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int wordsLength,
            /* [in][size_is] */ unsigned int *words,
            IInspectable *baseInterface,
            /* [out] */ IInspectable **innerInterface,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 **value) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96FactoryVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory, CreateInstance)
        HRESULT ( STDMETHODCALLTYPE *CreateInstance )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory * This,
            IInspectable *baseInterface,
            /* [out] */ IInspectable **innerInterface,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 **value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory, CreateInstance2)
        HRESULT ( STDMETHODCALLTYPE *CreateInstance2 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int word0,
            /* [in] */ unsigned int word1,
            /* [in] */ unsigned int word2,
            IInspectable *baseInterface,
            /* [out] */ IInspectable **innerInterface,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 **value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory, CreateInstance3)
        HRESULT ( STDMETHODCALLTYPE *CreateInstance3 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int wordsLength,
            /* [in][size_is] */ unsigned int *words,
            IInspectable *baseInterface,
            /* [out] */ IInspectable **innerInterface,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96 **value);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96FactoryVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96FactoryVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory_CreateInstance(This,baseInterface,innerInterface,value)	\
    ( (This)->lpVtbl -> CreateInstance(This,baseInterface,innerInterface,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory_CreateInstance2(This,timestamp,word0,word1,word2,baseInterface,innerInterface,value)	\
    ( (This)->lpVtbl -> CreateInstance2(This,timestamp,word0,word1,word2,baseInterface,innerInterface,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory_CreateInstance3(This,timestamp,wordsLength,words,baseInterface,innerInterface,value)	\
    ( (This)->lpVtbl -> CreateInstance3(This,timestamp,wordsLength,words,baseInterface,innerInterface,value) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage96Factory_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


