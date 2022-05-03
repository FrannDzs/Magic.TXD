#ifndef _NATIVE_EXECUTIVE_MAIN_HEADER_
#define _NATIVE_EXECUTIVE_MAIN_HEADER_

#include <assert.h>
#include <algorithm>

#include "CommonUtils.h"
#include "CExecutiveManager.h"

#include "internal/CExecutiveManager.internal.h"
#include "internal/CExecutiveManager.rwlock.internal.h"

#include <sdk/PluginHelpers.h>

#include "NativeUtils.h"

// High precision math wrap.
// Use it if you are encountering floating point precision issues.
// This wrap is used in timing critical code.
// TODO: we do not actually care on Linux.
struct HighPrecisionMathWrap
{
#ifdef _MSC_VER
    unsigned int _oldFPUVal;

    inline HighPrecisionMathWrap( void )
    {
        _oldFPUVal = _controlfp( 0, 0 );

        _controlfp( _PC_64, _MCW_PC );
    }

    inline ~HighPrecisionMathWrap( void )
    {
        _controlfp( _oldFPUVal, _MCW_PC );
    }
#endif //_MSC_VER
};

#endif //_NATIVE_EXECUTIVE_MAIN_HEADER_
