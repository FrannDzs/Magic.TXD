/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.Utils.hxx
*  PURPOSE:     FileSystem common utilities
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSUTILS_
#define _FILESYSUTILS_

#include "CFileSystem.common.h"
#include "fsinternal/CFileSystem.internal.common.h"

#include <sdk/GlobPattern.h>

// Default always-allow path character checker.
struct pathcheck_always_allow
{
    template <typename ucpType>
    AINLINE bool IsInvalidCharacter( ucpType cp ) const
    {
        return false;
    }
};

// Utility function to create a directory tree vector out of a relative path string.
// Ex.: 'my/path/to/desktop/' -> 'my', 'path', 'to', 'desktop'; file == false
//      'my/path/to/file' -> 'my', 'path', 'to', 'file'; file == true
// Could throw codepoint_exception if path is encoded wrongly.
template <typename charType, typename pathCheckerType>
inline normalNodePath _File_NormalizeRelativePath( const charType *path, const pathCheckerType& pathChecker, bool allowFile = true )
{
    normalNodePath nodePath;

    filePath entry;
    // it is not required to reserve memory anymore, because our NativeHeapAllocator is very optimized in resizing allocations.

    typedef character_env <charType> char_env;

    for ( character_env_iterator_tozero <charType> iter( path ); !iter.IsEnd(); iter.Increment() )
    {
        typename char_env::ucp_t curChar = iter.Resolve();

        if ( pathChecker.IsInvalidCharacter( curChar ) )
        {
            throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::ILLEGAL_PATHCHAR );
        }

        switch( curChar )
        {
        case '\\':
        case '/':
            if ( entry.empty() )
                break;

            // TODO: we also have this dot and double-dot stuff at ScanDirectory kind-of logic.
            //  unionize the data base of the two.

            if ( entry == "." )
            {
                // We ignore this current path indicator

                // Collect the next entry name.
                entry.clear();
            }
            else if ( entry == ".." )
            {
                if ( nodePath.travelNodes.GetCount() == 0 )
                {
                    nodePath.backCount++;
                }
                else
                {
                    nodePath.travelNodes.RemoveFromBack();
                }

                // Collect the next entry name.
                entry.clear();
            }
            else
            {
                nodePath.travelNodes.AddToBack( std::move( entry ) );
            }

            break;
        default:
            entry += curChar;
            break;
        }
    }

    // It is correct to not resolve the ".." if there is no slash character because
    // in that case it means a file.

    if ( !entry.empty() )
    {
        if ( entry == "." )
        {
            nodePath.isFilePath = false;
        }
        else if ( entry == ".." )
        {
            if ( nodePath.travelNodes.GetCount() == 0 )
            {
                nodePath.backCount++;
            }
            else
            {
                nodePath.travelNodes.RemoveFromBack();
            }

            nodePath.isFilePath = false;
        }
        else if ( allowFile )
        {
            nodePath.travelNodes.AddToBack( std::move( entry ) );
            nodePath.isFilePath = true;
        }
        else
        {
            nodePath.isFilePath = false;
        }
    }
    else
    {
        nodePath.isFilePath = false;
    }

    return nodePath;
}

// Commit the normalNodePath into a dirNames.
static AINLINE bool _File_CommitNodePathNoBreakout( normalNodePath& normalPath, bool& isFileOut, dirNames& commitTo )
{
    size_t oldNodeCount = commitTo.GetCount();

    size_t curBackCount = normalPath.backCount;
    size_t curNodeCount = oldNodeCount;

    if ( curBackCount > curNodeCount )
    {
        return false;
    }

    // Optimization.
    if ( curNodeCount == 0 )
    {
        commitTo = std::move( normalPath.travelNodes );
    }
    else
    {
        curNodeCount -= curBackCount;

        size_t newNodeCount = normalPath.travelNodes.GetCount();

        // Set correct length of path.
        size_t reqNodeCount = ( curNodeCount + newNodeCount );

        commitTo.Resize( reqNodeCount );

        for ( size_t n = 0; n < newNodeCount; n++ )
        {
            commitTo[ curNodeCount + n ] = std::move( normalPath.travelNodes[ n ] );
        }
    }

    isFileOut = normalPath.isFilePath;

    return true;
}

// Converts a normalNodePath into a SDK-type dirNames.
static AINLINE dirNames _File_ConvertNodePathToNodes( normalNodePath&& nodePath )
{
    dirNames result;

    size_t backCount = nodePath.backCount;

    for ( size_t n = 0; n < backCount; n++ )
    {
        result.AddToBack( ".." );
    }

    result.WriteVectorIntoCount( result.GetCount(), std::move( nodePath.travelNodes ), nodePath.travelNodes.GetCount() );

    return result;
}

// Output a node path into a filePath output.
// This is the reverse of _File_NormalizeRelativePath.
static AINLINE void _File_OutputPathTreeCount( const dirNames& nodes, size_t nodeCount, bool file, bool trueForwardFalseBackwardSlash, filePath& output )
{
    if ( file )
    {
        if ( nodeCount == 0 )
            return;

        nodeCount--;
    }

    for ( size_t n = 0; n < nodeCount; n++ )
    {
        output += nodes[n];
        output += FileSystem::GetDirectorySeparator <char> ( trueForwardFalseBackwardSlash );
    }

    if ( file )
    {
        output += nodes.GetBack();
    }
}

static AINLINE void _File_OutputPathTree( const dirNames& nodes, bool file, bool trueForwardFalseBackwardSlash, filePath& output )
{
    size_t actualDirEntries = nodes.GetCount();

    return _File_OutputPathTreeCount( nodes, actualDirEntries, file, trueForwardFalseBackwardSlash, output );
}

// Outputs a normalNodePath into a string.
static AINLINE filePath _File_OutputNormalPath( const normalNodePath& path, bool trueForwardFalseBackwardSlash )
{
    filePath output;

    // First we output the backward slashes.
    size_t backCount = path.backCount;

    for ( size_t n = 0; n < backCount; n++ )
    {
        output += "..";
        output += FileSystem::GetDirectorySeparator <char> ( trueForwardFalseBackwardSlash );
    }

    // Then we just do the normal path.
    _File_OutputPathTree( path.travelNodes, path.isFilePath, trueForwardFalseBackwardSlash, output );

    return output;
}

template <typename charType>
inline bool _File_IgnoreDirectoryScanEntry(
    bool ignoreCurrentDirDesc,
    bool ignoreParentDirDesc,
    const charType *name
)
{
    bool isFirstDot = ( name[0] == FileSystem::GetDotCharacter <charType> () );

    if ( ignoreCurrentDirDesc )
    {
        if ( isFirstDot && name[1] == (charType)0 )
        {
            return true;
        }
    }

    if ( ignoreParentDirDesc )
    {
        if ( isFirstDot && name[1] == FileSystem::GetDotCharacter <charType> () && name[2] == (charType)0 )
        {
            return true;
        }
    }

    return false;
}

template <typename allocatorType>
inline bool _File_IgnoreDirectoryScanEntry(
    bool ignoreCurrentDirDesc,
    bool ignoreParentDirDesc,
    const eir::MultiString <allocatorType>& name
)
{
    return name.char_dispatch(
        [&]( auto *name )
    {
        return _File_IgnoreDirectoryScanEntry( ignoreCurrentDirDesc, ignoreParentDirDesc, name );
    });
}

// Function called when any implementation finds a directory.
template <typename charType, typename patternCharType, typename allocatorType>
inline bool _File_DoesDirectoryMatch(
    const eir::PathPatternEnv <patternCharType, allocatorType>& patternEnv,
    const typename eir::PathPatternEnv <patternCharType, allocatorType>::filePattern_t& pattern,
    const charType *entryName
)
{
    bool allowDir = true;

    if ( !fileSystem->m_includeAllDirsInScan )
    {
        if ( patternEnv.MatchPattern( entryName, pattern ) == false )
        {
            allowDir = false;
        }
    }

    return allowDir;
}

template <typename charType, typename allocatorType, typename msAllocatorType>
inline bool _File_DoesDirectoryMatch(
    const eir::PathPatternEnv <charType, allocatorType>& patternEnv,
    const typename eir::PathPatternEnv <charType, allocatorType>::filePattern_t& pattern,
    const eir::MultiString <msAllocatorType>& entryName
)
{
    return entryName.char_dispatch(
        [&]( auto *entryName )
    {
        return _File_DoesDirectoryMatch( patternEnv, pattern, entryName );
    });
}

#endif //_FILESYSUTILS_
