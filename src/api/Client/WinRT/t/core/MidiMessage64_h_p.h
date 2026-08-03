

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for C:\Users\Pete\AppData\Local\Temp\MidiMessage64.idl-248f4f89:
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

#ifndef __MidiMessage64_h_p_h__
#define __MidiMessage64_h_p_h__

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

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64_FWD_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics_FWD_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory_FWD_DEFINED__ */


/* header files for imported files */
#include "Windows.Foundation.h"
#include "MidiApiContracts.h"
#include "IMidiUniversalPacket.h"
#include "MidiMessageStruct.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_MidiMessage64_0000_0000 */
/* [local] */ 





#pragma once 



extern RPC_IF_HANDLE __MIDL_itf_MidiMessage64_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiMessage64_0000_0000_v0_0_s_ifspec;

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-dd0010004064")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 : public IInspectable
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
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Vtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64, get_Word0)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Word0 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 * This,
            /* [retval][out] */ unsigned int *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64, put_Word0)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_Word0 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 * This,
            /* [in] */ unsigned int value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64, get_Word1)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Word1 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 * This,
            /* [retval][out] */ unsigned int *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64, put_Word1)
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_Word1 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 * This,
            /* [in] */ unsigned int value);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Vtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64_get_Word0(This,value)	\
    ( (This)->lpVtbl -> get_Word0(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64_put_Word0(This,value)	\
    ( (This)->lpVtbl -> put_Word0(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64_get_Word1(This,value)	\
    ( (This)->lpVtbl -> get_Word1(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64_put_Word1(This,value)	\
    ( (This)->lpVtbl -> put_Word1(This,value) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64_INTERFACE_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-ee0010004064")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics : public IInspectable
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE CreateFromStruct( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ struct __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageStruct message,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 **result) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64StaticsVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics, CreateFromStruct)
        HRESULT ( STDMETHODCALLTYPE *CreateFromStruct )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ struct __x_ABI_CWindows_CDevices_CMidi2_CMidiMessageStruct message,
            /* [retval][out] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 **result);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64StaticsVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64StaticsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics_CreateFromStruct(This,timestamp,message,result)	\
    ( (This)->lpVtbl -> CreateFromStruct(This,timestamp,message,result) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Statics_INTERFACE_DEFINED__ */


#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("d97ea0f4-3f1c-542b-bbdb-9e9a8d60f3c9")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory : public IInspectable
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE CreateInstance( 
            IInspectable *baseInterface,
            /* [out] */ IInspectable **innerInterface,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 **value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE CreateInstance2( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int word0,
            /* [in] */ unsigned int word1,
            IInspectable *baseInterface,
            /* [out] */ IInspectable **innerInterface,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 **value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE CreateInstance3( 
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int wordsLength,
            /* [in][size_is] */ unsigned int *words,
            IInspectable *baseInterface,
            /* [out] */ IInspectable **innerInterface,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 **value) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64FactoryVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory, CreateInstance)
        HRESULT ( STDMETHODCALLTYPE *CreateInstance )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory * This,
            IInspectable *baseInterface,
            /* [out] */ IInspectable **innerInterface,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 **value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory, CreateInstance2)
        HRESULT ( STDMETHODCALLTYPE *CreateInstance2 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int word0,
            /* [in] */ unsigned int word1,
            IInspectable *baseInterface,
            /* [out] */ IInspectable **innerInterface,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 **value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory, CreateInstance3)
        HRESULT ( STDMETHODCALLTYPE *CreateInstance3 )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory * This,
            /* [in] */ unsigned __int64 timestamp,
            /* [in] */ unsigned int wordsLength,
            /* [in][size_is] */ unsigned int *words,
            IInspectable *baseInterface,
            /* [out] */ IInspectable **innerInterface,
            /* [out][retval] */ __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64 **value);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64FactoryVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64FactoryVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory_CreateInstance(This,baseInterface,innerInterface,value)	\
    ( (This)->lpVtbl -> CreateInstance(This,baseInterface,innerInterface,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory_CreateInstance2(This,timestamp,word0,word1,baseInterface,innerInterface,value)	\
    ( (This)->lpVtbl -> CreateInstance2(This,timestamp,word0,word1,baseInterface,innerInterface,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory_CreateInstance3(This,timestamp,wordsLength,words,baseInterface,innerInterface,value)	\
    ( (This)->lpVtbl -> CreateInstance3(This,timestamp,wordsLength,words,baseInterface,innerInterface,value) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiMessage64Factory_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


