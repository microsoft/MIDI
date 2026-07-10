

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Mon Jan 18 22:14:07 2038
 */
/* Compiler settings for C:\Users\Pete\AppData\Local\Temp\MidiClock.idl-ca5a7481:
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

#ifndef __MidiClock_h_p_h__
#define __MidiClock_h_p_h__

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

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_FWD_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics;

#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_FWD_DEFINED__ */


/* header files for imported files */
#include "Windows.Foundation.h"
#include "MidiApiContracts.h"
#include "MidiSystemTimerSettings.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_MidiClock_0000_0000 */
/* [local] */ 



#pragma once 


extern RPC_IF_HANDLE __MIDL_itf_MidiClock_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_MidiClock_0000_0000_v0_0_s_ifspec;

#ifndef ____x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics */
/* [object][uuid] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8087b303-0519-c0de-31d1-ee0010003000")
    __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics : public IInspectable
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Now( 
            /* [retval][out] */ unsigned __int64 *value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_TimestampConstantSendImmediately( 
            /* [retval][out] */ unsigned __int64 *value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_TimestampConstantMessageQueueMaximumFutureTicks( 
            /* [retval][out] */ unsigned __int64 *value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_TimestampFrequency( 
            /* [retval][out] */ unsigned __int64 *value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ConvertTimestampTicksToNanoseconds( 
            /* [in] */ unsigned __int64 timestampValue,
            /* [retval][out] */ double *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ConvertTimestampTicksToMicroseconds( 
            /* [in] */ unsigned __int64 timestampValue,
            /* [retval][out] */ double *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ConvertTimestampTicksToMilliseconds( 
            /* [in] */ unsigned __int64 timestampValue,
            /* [retval][out] */ double *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ConvertTimestampTicksToSeconds( 
            /* [in] */ unsigned __int64 timestampValue,
            /* [retval][out] */ double *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OffsetTimestampByTicks( 
            /* [in] */ unsigned __int64 timestampValue,
            /* [in] */ __int64 offsetTicks,
            /* [retval][out] */ unsigned __int64 *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OffsetTimestampByMicroseconds( 
            /* [in] */ unsigned __int64 timestampValue,
            /* [in] */ __int64 offsetMicroseconds,
            /* [retval][out] */ unsigned __int64 *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OffsetTimestampByMilliseconds( 
            /* [in] */ unsigned __int64 timestampValue,
            /* [in] */ __int64 offsetMilliseconds,
            /* [retval][out] */ unsigned __int64 *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OffsetTimestampBySeconds( 
            /* [in] */ unsigned __int64 timestampValue,
            /* [in] */ __int64 offsetSeconds,
            /* [retval][out] */ unsigned __int64 *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetCurrentSystemTimerInfo( 
            /* [retval][out] */ struct __x_ABI_CWindows_CDevices_CMidi2_CMidiSystemTimerSettings *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE BeginLowLatencySystemTimerPeriod( 
            /* [retval][out] */ boolean *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE EndLowLatencySystemTimerPeriod( 
            /* [retval][out] */ boolean *result) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStaticsVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This);
        
        DECLSPEC_XFGVIRT(IInspectable, GetIids)
        HRESULT ( STDMETHODCALLTYPE *GetIids )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [out] */ ULONG *iidCount,
            /* [size_is][size_is][out] */ IID **iids);
        
        DECLSPEC_XFGVIRT(IInspectable, GetRuntimeClassName)
        HRESULT ( STDMETHODCALLTYPE *GetRuntimeClassName )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [out] */ HSTRING *className);
        
        DECLSPEC_XFGVIRT(IInspectable, GetTrustLevel)
        HRESULT ( STDMETHODCALLTYPE *GetTrustLevel )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [out] */ TrustLevel *trustLevel);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics, get_Now)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Now )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [retval][out] */ unsigned __int64 *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics, get_TimestampConstantSendImmediately)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_TimestampConstantSendImmediately )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [retval][out] */ unsigned __int64 *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics, get_TimestampConstantMessageQueueMaximumFutureTicks)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_TimestampConstantMessageQueueMaximumFutureTicks )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [retval][out] */ unsigned __int64 *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics, get_TimestampFrequency)
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_TimestampFrequency )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [retval][out] */ unsigned __int64 *value);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics, ConvertTimestampTicksToNanoseconds)
        HRESULT ( STDMETHODCALLTYPE *ConvertTimestampTicksToNanoseconds )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [in] */ unsigned __int64 timestampValue,
            /* [retval][out] */ double *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics, ConvertTimestampTicksToMicroseconds)
        HRESULT ( STDMETHODCALLTYPE *ConvertTimestampTicksToMicroseconds )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [in] */ unsigned __int64 timestampValue,
            /* [retval][out] */ double *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics, ConvertTimestampTicksToMilliseconds)
        HRESULT ( STDMETHODCALLTYPE *ConvertTimestampTicksToMilliseconds )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [in] */ unsigned __int64 timestampValue,
            /* [retval][out] */ double *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics, ConvertTimestampTicksToSeconds)
        HRESULT ( STDMETHODCALLTYPE *ConvertTimestampTicksToSeconds )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [in] */ unsigned __int64 timestampValue,
            /* [retval][out] */ double *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics, OffsetTimestampByTicks)
        HRESULT ( STDMETHODCALLTYPE *OffsetTimestampByTicks )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [in] */ unsigned __int64 timestampValue,
            /* [in] */ __int64 offsetTicks,
            /* [retval][out] */ unsigned __int64 *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics, OffsetTimestampByMicroseconds)
        HRESULT ( STDMETHODCALLTYPE *OffsetTimestampByMicroseconds )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [in] */ unsigned __int64 timestampValue,
            /* [in] */ __int64 offsetMicroseconds,
            /* [retval][out] */ unsigned __int64 *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics, OffsetTimestampByMilliseconds)
        HRESULT ( STDMETHODCALLTYPE *OffsetTimestampByMilliseconds )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [in] */ unsigned __int64 timestampValue,
            /* [in] */ __int64 offsetMilliseconds,
            /* [retval][out] */ unsigned __int64 *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics, OffsetTimestampBySeconds)
        HRESULT ( STDMETHODCALLTYPE *OffsetTimestampBySeconds )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [in] */ unsigned __int64 timestampValue,
            /* [in] */ __int64 offsetSeconds,
            /* [retval][out] */ unsigned __int64 *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics, GetCurrentSystemTimerInfo)
        HRESULT ( STDMETHODCALLTYPE *GetCurrentSystemTimerInfo )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [retval][out] */ struct __x_ABI_CWindows_CDevices_CMidi2_CMidiSystemTimerSettings *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics, BeginLowLatencySystemTimerPeriod)
        HRESULT ( STDMETHODCALLTYPE *BeginLowLatencySystemTimerPeriod )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [retval][out] */ boolean *result);
        
        DECLSPEC_XFGVIRT(__x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics, EndLowLatencySystemTimerPeriod)
        HRESULT ( STDMETHODCALLTYPE *EndLowLatencySystemTimerPeriod )( 
            __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics * This,
            /* [retval][out] */ boolean *result);
        
        END_INTERFACE
    } __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStaticsVtbl;

    interface __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics
    {
        CONST_VTBL struct __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStaticsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_GetIids(This,iidCount,iids)	\
    ( (This)->lpVtbl -> GetIids(This,iidCount,iids) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_GetRuntimeClassName(This,className)	\
    ( (This)->lpVtbl -> GetRuntimeClassName(This,className) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_GetTrustLevel(This,trustLevel)	\
    ( (This)->lpVtbl -> GetTrustLevel(This,trustLevel) ) 


#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_get_Now(This,value)	\
    ( (This)->lpVtbl -> get_Now(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_get_TimestampConstantSendImmediately(This,value)	\
    ( (This)->lpVtbl -> get_TimestampConstantSendImmediately(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_get_TimestampConstantMessageQueueMaximumFutureTicks(This,value)	\
    ( (This)->lpVtbl -> get_TimestampConstantMessageQueueMaximumFutureTicks(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_get_TimestampFrequency(This,value)	\
    ( (This)->lpVtbl -> get_TimestampFrequency(This,value) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_ConvertTimestampTicksToNanoseconds(This,timestampValue,result)	\
    ( (This)->lpVtbl -> ConvertTimestampTicksToNanoseconds(This,timestampValue,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_ConvertTimestampTicksToMicroseconds(This,timestampValue,result)	\
    ( (This)->lpVtbl -> ConvertTimestampTicksToMicroseconds(This,timestampValue,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_ConvertTimestampTicksToMilliseconds(This,timestampValue,result)	\
    ( (This)->lpVtbl -> ConvertTimestampTicksToMilliseconds(This,timestampValue,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_ConvertTimestampTicksToSeconds(This,timestampValue,result)	\
    ( (This)->lpVtbl -> ConvertTimestampTicksToSeconds(This,timestampValue,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_OffsetTimestampByTicks(This,timestampValue,offsetTicks,result)	\
    ( (This)->lpVtbl -> OffsetTimestampByTicks(This,timestampValue,offsetTicks,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_OffsetTimestampByMicroseconds(This,timestampValue,offsetMicroseconds,result)	\
    ( (This)->lpVtbl -> OffsetTimestampByMicroseconds(This,timestampValue,offsetMicroseconds,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_OffsetTimestampByMilliseconds(This,timestampValue,offsetMilliseconds,result)	\
    ( (This)->lpVtbl -> OffsetTimestampByMilliseconds(This,timestampValue,offsetMilliseconds,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_OffsetTimestampBySeconds(This,timestampValue,offsetSeconds,result)	\
    ( (This)->lpVtbl -> OffsetTimestampBySeconds(This,timestampValue,offsetSeconds,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_GetCurrentSystemTimerInfo(This,result)	\
    ( (This)->lpVtbl -> GetCurrentSystemTimerInfo(This,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_BeginLowLatencySystemTimerPeriod(This,result)	\
    ( (This)->lpVtbl -> BeginLowLatencySystemTimerPeriod(This,result) ) 

#define __x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_EndLowLatencySystemTimerPeriod(This,result)	\
    ( (This)->lpVtbl -> EndLowLatencySystemTimerPeriod(This,result) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CDevices_CMidi2_CIMidiClockStatics_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


