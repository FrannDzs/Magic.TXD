/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/MacroUtils.h
*  PURPOSE:     Common macros in the SDK
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _COMMON_MACRO_UTILITIES_
#define _COMMON_MACRO_UTILITIES_

// Basic always inline definition.
#ifndef AINLINE
#ifdef _MSC_VER
#define AINLINE __forceinline
#elif defined(__linux__)
#define AINLINE inline
#else
#define AINLINE inline
#endif
#endif

#ifndef LAINLINE
#ifdef _MSC_VER
#define LAINLINE [[msvc::forceinline]]
#elif defined(__linux__)
#define LAINLINE __attribute__((always_inline))
#else
#define LAINLINE
#endif
#endif

#ifndef _MSC_VER
#define abstract
#endif

#ifndef countof
#define countof(x) (sizeof(x)/sizeof(*x))
#endif

#endif //_COMMON_MACRO_UTILITIES_