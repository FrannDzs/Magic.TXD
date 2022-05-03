/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/include/CFileSystem.common.h
*  PURPOSE:     Common definitions of the FileSystem module
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_COMMON_DEFINITIONS_
#define _FILESYSTEM_COMMON_DEFINITIONS_

#include <stdint.h>

#include <sdk/MemoryRaw.h>
#include <sdk/Vector.h>
#include <sdk/Set.h>
#include <sdk/Map.h>

#include "CFileSystem.common.filePath.h"

// Definition of the offset type that accomodates for all kinds of files.
// Realistically speaking, the system is not supposed to have files that are bigger than
// this number type can handle.
// Use this number type whenever correct operations are required.
// REMEMBER: be sure to check that your compiler supports the number representation you want!
typedef long long int fsOffsetNumber_t;

// Types used to keep binary compatibility between compiler implementations and architectures.
// Binary compatibility is required in streams, so that reading streams is not influenced by
// compilation mode.
typedef bool fsBool_t;
typedef char fsChar_t;
typedef unsigned char fsUChar_t;
typedef int16_t fsShort_t;
typedef uint16_t fsUShort_t;
typedef int32_t fsInt_t;
typedef uint32_t fsUInt_t;
typedef int64_t fsWideInt_t;
typedef uint64_t fsUWideInt_t;
typedef float fsFloat_t;
typedef double fsDouble_t;

template <typename charType>
using fsStaticString = eir::String <charType, FileSysCommonAllocator>;

template <typename structType>
using fsStaticVector = eir::Vector <structType, FileSysCommonAllocator>;

template <typename keyType, typename valueType, typename comparatorType = eir::MapDefaultComparator>
using fsStaticMap = eir::Map <keyType, valueType, FileSysCommonAllocator, comparatorType>;

template <typename valueType, typename comparatorType = eir::SetDefaultComparator>
using fsStaticSet = eir::Set <valueType, FileSysCommonAllocator, comparatorType>;

// Compiler compatibility
#ifndef _MSC_VER
#define abstract
#endif //_MSC:VER

#ifdef __linux__
#include <sys/stat.h>

#define _strdup strdup
#define _vsnprintf vsnprintf
#endif //__linux__

// List of directory names.
// Used as storage for a parsed file path, each directory node added to it.
typedef eir::Vector <filePath, FileSysCommonAllocator> dirNames;

// Normal form of a (relative) path.
struct normalNodePath
{
    size_t backCount = 0;       // number of ".." entries before the path.
    dirNames travelNodes;       // node-names to go along into.
    bool isFilePath = false;
};

// Default data buffer used in virtual interfaces.
typedef eir::Vector <char, FileSysCommonAllocator> fsDataBuffer;

enum eFileException
{
    FILE_STREAM_TERMINATED  // user attempts to perform on a terminated file stream, ie. http timeout
};

// Failure codes for opening files.
enum class eFileOpenFailure
{
    NONE,
    UNKNOWN_ERROR,
    PATH_OUT_OF_SCOPE,
    INVALID_PARAMETERS,
    RESOURCES_EXHAUSTED,
    ACCESS_DENIED,
    NOT_FOUND,
    ALREADY_EXISTS
};

#ifndef NUMELMS
#define NUMELMS(x)      ( sizeof(x) / sizeof(*x) )
#endif //NUMELMS

#endif //_FILESYSTEM_COMMON_DEFINITIONS_