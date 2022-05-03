/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.translator.pathutil.h
*  PURPOSE:     FileSystem path utilities for all kinds of translators
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_TRANSLATOR_PATHUTIL_
#define _FILESYSTEM_TRANSLATOR_PATHUTIL_

#ifdef FILESYS_MULTI_THREADING
#include <CExecutiveManager.h>
#endif //FILESYS_MULTI_THREADING

// Well shit. Including it into global namespace of FileSystem I guess.
// Happens sometimes that design decisions change.
#include "../CFileSystem.Utils.hxx"

/*=====================================================
    CSystemPathTranslator

    Filesystem path translation utility. Handles path input
    at any stage so implementations can use GetFullPath
    family of functions to query the real paths into their
    FS trees.

    Implementations had to be moved into the header because
    template specializations became important, most
    specifically for full path handling.

    pathCheckerType is used to validate characters inside
    any user-provided path (example: pathcheck_always_allow).
=====================================================*/

template <typename pathCheckerType, typename systemPathType>
class CSystemPathTranslator : public virtual CFileTranslator
{
public:
    CSystemPathTranslator( systemPathType rootPath, bool isSystemPath, bool isCaseSensitive )
        : m_rootPath( std::move( rootPath ) )
    {
        m_isSystemPath = isSystemPath;
        m_isCaseSensitive = isCaseSensitive;
        m_enableOutbreak = false;
        m_pathProcessMode = fileSystem->m_defaultPathProcessMode;

#ifdef FILESYS_MULTI_THREADING
        lockPathConsistency = MakeReadWriteLock( fileSystem );
#endif //FILESYS_MULTI_THREADING
    }

    ~CSystemPathTranslator( void )
    {
#ifdef FILESYS_MULTI_THREADING
        DeleteReadWriteLock( fileSystem, lockPathConsistency );
#endif //FILESYS_MULTI_THREADING
    }

private:
    // *** Generic path resolution methods that work on any character type.
    //     It is our special choice which character types we expose natively, anyway.

    template <typename charType>
    bool            GenGetFullPathNodesFromRoot     ( const charType *path, normalNodePath& nodesOut ) const
    {
#ifdef FILESYS_MULTI_THREADING
        NativeExecutive::CReadWriteReadContextSafe <> consistency( this->lockPathConsistency );
#endif //FILESYS_MULTI_THREADING

        return IntGetFullPathNodesFromRoot( path, true, nodesOut );
    }
    template <typename charType>
    bool            GenGetFullPathNodes             ( const charType *path, normalNodePath& nodesOut ) const
    {
#ifdef FILESYS_MULTI_THREADING
        NativeExecutive::CReadWriteReadContextSafe <> consistency( this->lockPathConsistency );
#endif //FILESYS_MULTI_THREADING

        return IntGetFullPathNodes( path, true, nodesOut );
    }
    template <typename charType>
    bool            GenGetRelativePathNodesFromRoot ( const charType *path, normalNodePath& nodesOut ) const
    {
#ifdef FILESYS_MULTI_THREADING
        NativeExecutive::CReadWriteReadContextSafe <> consistency( this->lockPathConsistency );
#endif //FILESYS_MULTI_THREADING

        return IntGetRelativePathNodesFromRoot( path, true, nodesOut );
    }
    template <typename charType>
    bool            GenGetRelativePathNodes         ( const charType *path, normalNodePath& nodesOut ) const
    {
#ifdef FILESYS_MULTI_THREADING
        NativeExecutive::CReadWriteReadContextSafe <> consistency( this->lockPathConsistency );
#endif //FILESYS_MULTI_THREADING

        return IntGetRelativePathNodes( path, true, nodesOut );
    }
    template <typename charType>
    bool            GenGetFullPathFromRoot          ( const charType *path, bool allowFile, filePath& output ) const
    {
#ifdef FILESYS_MULTI_THREADING
        NativeExecutive::CReadWriteReadContextSafe <> consistency( this->lockPathConsistency );
#endif //FILESYS_MULTI_THREADING

        filePath rootDesc;
        normalNodePath normalPath;
        bool slashDir;

        bool resolvedNodes = IntGetFullPathNodesFromRoot( path, allowFile, normalPath, &rootDesc, &slashDir );

        if ( !resolvedNodes )
            return false;

        _File_OutputPathTree( normalPath.travelNodes, normalPath.isFilePath, slashDir, rootDesc );

        output = std::move( rootDesc );
        return true;
    }
    template <typename charType>
    bool            GenGetFullPath                  ( const charType *path, bool allowFile, filePath& output ) const
    {
        filePath rootDesc;
        normalNodePath normalPath;
        bool slashDir;

#ifdef FILESYS_MULTI_THREADING
        NativeExecutive::CReadWriteReadContextSafe <> consistency( this->lockPathConsistency );
#endif //FILESYS_MULTI_THREADING

        if ( !IntGetFullPathNodes( path, allowFile, normalPath, &rootDesc, &slashDir ) )
            return false;

        _File_OutputPathTree( normalPath.travelNodes, normalPath.isFilePath, slashDir, rootDesc );

        output = std::move( rootDesc );

        // We are done.
        return true;
    }
    template <typename charType>
    bool            GenGetRelativePathFromRoot      ( const charType *path, bool allowFile, filePath& output ) const
    {
#ifdef FILESYS_MULTI_THREADING
        NativeExecutive::CReadWriteReadContextSafe <> consistency( this->lockPathConsistency );
#endif //FILESYS_MULTI_THREADING

        normalNodePath normalPath;

        bool couldResolve = IntGetRelativePathNodesFromRoot( path, allowFile, normalPath );

        if ( !couldResolve )
            return false;

        output = std::move( _File_OutputNormalPath( normalPath, true ) );
        return true;
    }
    template <typename charType>
    bool            GenGetRelativePath              ( const charType *path, bool allowFile, filePath& output ) const
    {
#ifdef FILESYS_MULTI_THREADING
        NativeExecutive::CReadWriteReadContextSafe <> consistency( this->lockPathConsistency );
#endif //FILESYS_MULTI_THREADING

        normalNodePath normalPath;

        bool couldResolve = IntGetRelativePathNodes( path, allowFile, normalPath );

        if ( !couldResolve )
            return false;

        output = std::move( _File_OutputNormalPath( normalPath, true ) );
        return true;
    }
    template <typename charType>
    bool            GenChangeDirectory              ( const charType *path )
    {
#ifdef FILESYS_MULTI_THREADING
        NativeExecutive::CReadWriteWriteContextSafe <> consistency( this->lockPathConsistency );
#endif //FILESYS_MULTI_THREADING

        // Check whether a directory can have files at the end.
        bool allowFileAtEnd = FileSystem::CanDirectoryPathHaveTrailingFile( this->m_pathProcessMode, filesysPathOperatingMode::DEFAULT );

        translatorPathResult transPath;

        if ( !ParseTranslatorPathGuided( path, this->m_curDirPath, eBaseDirDesignation::ROOT_DIR, allowFileAtEnd, transPath ) )
            return false;

        // If we can have files at the end and we are a directory path, hack back the trailing slash.
        if ( allowFileAtEnd && transPath.type == eRequestedPathResolution::RELATIVE_PATH )
        {
            transPath.relpath.isFilePath = false;
        }

        // Prepare the new current path.
        // This is important because the creation of the path could throw exceptions.
        // Also make sure to ignore isFilePath because it is just a descriptor of the trailing-slash.
        bool hasConfirmed = OnConfirmDirectoryChange( transPath );

        if ( hasConfirmed )
        {
            m_curDirPath = std::move( transPath );
        }

        return hasConfirmed;
    }

    // GetFullPathNodes
    // GetFullPathNodesFromRoot
    // GetRelativePathNodes
    // GetRelativePathNodesFromRoot

protected:
    enum class eRequestedPathResolution
    {
        RELATIVE_PATH,
        FULL_PATH
    };

    enum class eBaseDirDesignation
    {
        ROOT_DIR,       // path is appended to just root.
        CUR_DIR         // path is appended to current-dir and that is appended to root.
    };

    // Wrapper object for usage of systemPathType inside of PathSegmentation::PathSegmenter.
    struct systemPathView
    {
        AINLINE systemPathView( const systemPathType& sysPath ) : sysPath( sysPath )
        {
            return;
        }

        AINLINE size_t BackCount( void ) const
        {
            return 0;
        }

        AINLINE size_t ItemCount( void ) const
        {
            if ( sysPath.IsFilePath() )
            {
                return sysPath.RootNodes().GetCount() - 1;
            }

            return sysPath.RootNodes().GetCount();
        }

        AINLINE const filePath* GetItems( void ) const
        {
            return sysPath.RootNodes().GetData();
        }

        const systemPathType& sysPath;
    };

    enum class eParsedPathType
    {
        TRANSLATOR_ROOT,
        REL_CUR_DIR,
        FULL_PATH
    };

    struct parsedTranslatorPath
    {
        eParsedPathType parsedType;

        // Valid if TRANSLATOR_ROOT or REL_CUR_DIR.
        normalNodePath rel_normalPath;
        // Valid if FULL_PATH.
        systemPathType sys_fullPath;
    };

    // Parsed a translator path into a manageable container.
    template <typename charType>
    AINLINE parsedTranslatorPath DispatchTranslatorPath( const charType *path, bool allowFile ) const
    {
        // Try to detect a path from translator root.
        {
            const charType *pathAfterRootDesc;

            if ( this->IsTranslatorRootDescriptor( path, pathAfterRootDesc ) )
            {
                parsedTranslatorPath info;
                info.parsedType = eParsedPathType::TRANSLATOR_ROOT;
                info.rel_normalPath = _File_NormalizeRelativePath( pathAfterRootDesc, pathCheckerType(), allowFile );
                return info;
            }
        }

        // Try to detect path from filesystem root.
        {
            systemPathType sysFullPath;

            if ( sysFullPath.buildFromSystemPath( path, allowFile ) )
            {
                parsedTranslatorPath info;
                info.parsedType = eParsedPathType::FULL_PATH;
                info.sys_fullPath = std::move( sysFullPath );
                return info;
            }
        }

        // Has to be attempted as path from current dir of translator.
        parsedTranslatorPath info;
        info.parsedType = eParsedPathType::REL_CUR_DIR;
        info.rel_normalPath = _File_NormalizeRelativePath( path, pathCheckerType(), allowFile );
        return info;
    }

    // Determines whether the root descriptor is different than the translator's root descriptor.
    AINLINE bool HasRootDescriptorChanged( const parsedTranslatorPath& parsedPath ) const
    {
        if ( parsedPath.parsedType != eParsedPathType::FULL_PATH )
        {
            return false;
        }

        return ( parsedPath.sys_fullPath.doesRootDescriptorMatch( this->m_rootPath ) == false );
    }

    struct translatorPathResult
    {
        eRequestedPathResolution type = eRequestedPathResolution::RELATIVE_PATH;
        normalNodePath relpath;
        systemPathType fullpath;

        inline bool IsFilePath( void ) const
        {
            if ( this->type == eRequestedPathResolution::RELATIVE_PATH )
            {
                return relpath.isFilePath;
            }
            else if ( this->type == eRequestedPathResolution::FULL_PATH )
            {
                return fullpath.IsFilePath();
            }

            return false;
        }
    };
    
    AINLINE bool OutputTranslatorPath(
        parsedTranslatorPath&& parsedPath, eRequestedPathResolution outputType,
        const translatorPathResult& curDirPath, eBaseDirDesignation baseDirType,
        translatorPathResult& pathOut
    ) const
    {
        using namespace PathSegmentation;

        bool outbreakEnabled = this->m_enableOutbreak;

        eParsedPathType parsedType = parsedPath.parsedType;
        eRequestedPathResolution curDirType = curDirPath.type;

        if ( parsedType == eParsedPathType::FULL_PATH )
        {
            systemPathType& sysFullPath = parsedPath.sys_fullPath;

            // Construct the requested path.
            PathSegmenter <systemPathView> reqPath( sysFullPath );

            bool caseSensitive = this->m_isCaseSensitive;

            if ( curDirType == eRequestedPathResolution::RELATIVE_PATH || baseDirType == eBaseDirDesignation::ROOT_DIR )
            {
                size_t rootDirTravelCount;
                size_t travelIndex = 0;

                size_t rootDirTransRootSegmentEnd;

                if ( baseDirType == eBaseDirDesignation::ROOT_DIR || outputType == eRequestedPathResolution::FULL_PATH )
                {
                    if ( !( outputType == eRequestedPathResolution::FULL_PATH && outbreakEnabled ) )
                    {
                        // Construct our root directory.
                        PathSegmenter <helpers::fullDirNodesView> rootDir( this->m_rootPath.RootNodes() );

                        rootDirTravelCount = rootDir.GetTravelCount();

                        helpers::TravelPaths( travelIndex, rootDir, reqPath, caseSensitive );

                        rootDirTransRootSegmentEnd = rootDir.GetSegmentEndOffset( 0 );
                    }
                }
                else if ( baseDirType == eBaseDirDesignation::CUR_DIR )
                {
                    // Construct our root directory.
                    PathSegmenter <helpers::fullDirNodesView, helpers::normalPathView> rootDir( this->m_rootPath.RootNodes(), curDirPath.relpath );

                    rootDirTravelCount = rootDir.GetTravelCount();

                    helpers::TravelPaths( travelIndex, rootDir, reqPath, caseSensitive );

                    rootDirTransRootSegmentEnd = rootDir.GetSegmentEndOffset( 0 );
                }
                else
                {
                    throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
                }

                // Check for outbreak from translator root.
                if ( outbreakEnabled == false )
                {
                    if ( travelIndex < rootDirTransRootSegmentEnd )
                    {
                        return false;
                    }
                }

                // Return the path.
                translatorPathResult result;

                if ( outputType == eRequestedPathResolution::RELATIVE_PATH )
                {
                    normalNodePath normalPath;
                    normalPath.isFilePath = sysFullPath.IsFilePath();
                    normalPath.backCount = ( rootDirTravelCount - travelIndex );
                    normalPath.travelNodes.WriteVectorIntoCount( 0, std::move( sysFullPath.RootNodes() ), ( sysFullPath.RootNodes().GetCount() - travelIndex ), travelIndex );

                    result.type = eRequestedPathResolution::RELATIVE_PATH;
                    result.relpath = std::move( normalPath );
                }
                else if ( outputType == eRequestedPathResolution::FULL_PATH )
                {
                    result.type = eRequestedPathResolution::FULL_PATH;
                    result.fullpath = std::move( sysFullPath );
                }
                else
                {
                    throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
                }

                pathOut = std::move( result );

                return true;
            }
            else if ( baseDirType == eBaseDirDesignation::CUR_DIR && curDirType == eRequestedPathResolution::FULL_PATH )
            {
                PathSegmenter <systemPathView> rootPath( curDirPath.fullpath );

                size_t rootDirTravelCount = rootPath.GetTravelCount();

                size_t travelIndex = 0;

                if ( !( outputType == eRequestedPathResolution::FULL_PATH && outbreakEnabled ) )
                {
                    helpers::TravelPaths( travelIndex, rootPath, reqPath, caseSensitive );
                }

                // Just some imaginary logic, meow.
                // It does not hurt because usually outbreakEnabled is false anyway.
                if ( outbreakEnabled == false )
                {
                    if ( travelIndex < rootDirTravelCount )
                    {
                        return false;
                    }
                }

                translatorPathResult result;

                if ( outputType == eRequestedPathResolution::RELATIVE_PATH )
                {
                    result.type = eRequestedPathResolution::RELATIVE_PATH;
                    result.relpath.backCount = ( rootDirTravelCount - travelIndex );
                    result.relpath.travelNodes.WriteVectorIntoCount( 0, std::move( sysFullPath.RootNodes() ), ( sysFullPath.RootNodes().GetCount() - travelIndex ), travelIndex );
                    result.relpath.isFilePath = sysFullPath.IsFilePath();
                }
                else if ( outputType == eRequestedPathResolution::FULL_PATH )
                {
                    result.type = eRequestedPathResolution::FULL_PATH;
                    result.fullpath = std::move( sysFullPath );
                }
                else
                {
                    throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
                }

                pathOut = std::move( result );

                return true;
            }
        }
        else if ( parsedType == eParsedPathType::REL_CUR_DIR || parsedType == eParsedPathType::TRANSLATOR_ROOT )
        {
            bool isFilePath = parsedPath.rel_normalPath.isFilePath;

            if ( curDirType == eRequestedPathResolution::RELATIVE_PATH || baseDirType == eBaseDirDesignation::ROOT_DIR )
            {
                normalNodePath nullcurdir;

                // Construct the request path.
                PathSegmenter <helpers::fullDirNodesView, helpers::normalPathView, helpers::normalPathView> reqPath(
                    this->m_rootPath.RootNodes(),
                    ( parsedType == eParsedPathType::REL_CUR_DIR ? curDirPath.relpath : nullcurdir ),
                    parsedPath.rel_normalPath
                );

                // Does it break out of filesystem root?
                if ( reqPath.GetSegmentBackCount( 0 ) > 0 )
                {
                    // Invalid.
                    return false;
                }

                // Does it attempt to break out of translator root?
                if ( outbreakEnabled == false )
                {
                    if ( reqPath.GetSegmentEndOffset( 0 ) < this->m_rootPath.RootNodes().GetCount() )
                    {
                        // Broken out.
                        return false;
                    }
                }

                // Construct the result path.
                translatorPathResult result;

                if ( outputType == eRequestedPathResolution::FULL_PATH )
                {
                    dirNames nodes;
                    nodes.Insert( nodes.GetCount(), this->m_rootPath.RootNodes().GetData(), reqPath.GetSegmentTravelCount( 0 ) );
                    nodes.Insert( nodes.GetCount(), curDirPath.relpath.travelNodes.GetData(), reqPath.GetSegmentTravelCount( 1 ) );
                    nodes.WriteVectorIntoCount( nodes.GetCount(), std::move( parsedPath.rel_normalPath.travelNodes ), parsedPath.rel_normalPath.travelNodes.GetCount() );

                    result.type = eRequestedPathResolution::FULL_PATH;
                    result.fullpath.buildFromParams( this->m_rootPath, std::move( nodes ), isFilePath );
                }
                else if ( outputType == eRequestedPathResolution::RELATIVE_PATH )
                {
                    // Form the absolute path and then turn that into a relative path again, effectively calculating the shortest
                    // relative path.

                    bool caseSensitive = this->m_isCaseSensitive;

                    size_t travelIndex;
                    size_t baseDirTravelCount;
                    size_t baseDirRootSegmentEndOff;

                    if ( baseDirType == eBaseDirDesignation::ROOT_DIR )
                    {
                        PathSegmenter <helpers::fullDirNodesView> rootDir( this->m_rootPath.RootNodes() );

                        travelIndex = reqPath.GetSegmentStartIndex( 1 );

                        helpers::TravelPaths( travelIndex, rootDir, reqPath, caseSensitive );

                        baseDirTravelCount = rootDir.GetTravelCount();
                        baseDirRootSegmentEndOff = rootDir.GetSegmentEndOffset( 0 );
                    }
                    else if ( baseDirType == eBaseDirDesignation::CUR_DIR )
                    {
                        PathSegmenter <helpers::fullDirNodesView, helpers::normalPathView> rootDir( this->m_rootPath.RootNodes(), curDirPath.relpath );

                        travelIndex = std::min( reqPath.GetSegmentStartIndex( 1 ), rootDir.GetSegmentStartIndex( 1 ) );

                        helpers::TravelPaths( travelIndex, rootDir, reqPath, caseSensitive );

                        baseDirTravelCount = rootDir.GetTravelCount();
                        baseDirRootSegmentEndOff = rootDir.GetSegmentEndOffset( 0 );
                    }
                    else
                    {
                        throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
                    }

                    // Check that we do not break out of root if we are not set for outbreak.
                    if ( outbreakEnabled == false )
                    {
                        if ( travelIndex < baseDirRootSegmentEndOff )
                        {
                            return false;
                        }
                    }

                    // Calculate the amount of nodes from current iter of root path to the top.
                    normalNodePath normalPath;
                    normalPath.backCount = ( baseDirTravelCount - travelIndex );

                    // Return all the remaining nodes.
                    size_t inputCurDirIter = reqPath.ChopToSegment( 1, travelIndex );
                    size_t normalPathIter = reqPath.ChopToSegment( 2, travelIndex );

                    normalPath.travelNodes.Insert( normalPath.travelNodes.GetCount(), curDirPath.relpath.travelNodes.GetData() + inputCurDirIter, ( reqPath.GetSegmentTravelCount( 1 ) - inputCurDirIter ) );
                    normalPath.travelNodes.WriteVectorIntoCount( normalPath.travelNodes.GetCount(), std::move( parsedPath.rel_normalPath.travelNodes ), ( parsedPath.rel_normalPath.travelNodes.GetCount() - normalPathIter ), normalPathIter );
                    normalPath.isFilePath = isFilePath;

                    result.type = eRequestedPathResolution::RELATIVE_PATH;
                    result.relpath = std::move( normalPath );
                }
                else
                {
                    throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
                }

                pathOut = std::move( result );

                return true;
            }
            else if ( curDirType == eRequestedPathResolution::FULL_PATH && baseDirType == eBaseDirDesignation::CUR_DIR )
            {
                // Current-dir should only be full-path if the current-directory has a different root descriptor than
                // the root of translator. This can rarely happen, most often on the Windows platform.

                bool caseSensitive = this->m_isCaseSensitive;

                const systemPathType *usedRootPath;

                if ( parsedType == eParsedPathType::REL_CUR_DIR )
                {
                    usedRootPath = &curDirPath.fullpath;
                }
                else
                {
                    usedRootPath = &this->m_rootPath;
                }

                // Construct the request path.
                PathSegmenter <helpers::fullDirNodesView, helpers::normalPathView> reqPath( usedRootPath->RootNodes(), parsedPath.rel_normalPath );

                // Cannot break out of filesystem root.
                if ( reqPath.GetBackCount() > 0 )
                {
                    return false;
                }

                // Construct the root path.
                PathSegmenter <helpers::fullDirNodesView> rootPath( curDirPath.fullpath.RootNodes() );

                size_t travelIndex = reqPath.GetSegmentEndOffset( 0 );

                helpers::TravelPaths( travelIndex, rootPath, reqPath, caseSensitive );

                size_t rootTravelCount = rootPath.GetTravelCount();

                // Same as above just imaginary logic because we cannot have an outbreak translator whose current directory is
                // a full path.
                if ( outbreakEnabled == false )
                {
                    if ( travelIndex < rootTravelCount )
                    {
                        return false;
                    }
                }

                // Return the result.
                translatorPathResult result;

                if ( outputType == eRequestedPathResolution::FULL_PATH )
                {
                    dirNames rootNodes;
                    rootNodes.Insert( rootNodes.GetCount(), usedRootPath->RootNodes().GetData(), reqPath.GetSegmentTravelCount( 0 ) );
                    rootNodes.WriteVectorIntoCount( rootNodes.GetCount(), std::move( parsedPath.rel_normalPath.travelNodes ), parsedPath.rel_normalPath.travelNodes.GetCount() );

                    result.type = eRequestedPathResolution::FULL_PATH;
                    result.fullpath.buildFromParams( *usedRootPath, std::move( rootNodes ), parsedPath.rel_normalPath.isFilePath );
                }
                else if ( outputType == eRequestedPathResolution::RELATIVE_PATH )
                {
                    normalNodePath normalPath;
                    normalPath.backCount = ( rootTravelCount - travelIndex );
                    normalPath.travelNodes.WriteVectorIntoCount( normalPath.travelNodes.GetCount(), std::move( parsedPath.rel_normalPath.travelNodes ), parsedPath.rel_normalPath.travelNodes.GetCount() );
                    normalPath.isFilePath = parsedPath.rel_normalPath.isFilePath;

                    result.type = eRequestedPathResolution::RELATIVE_PATH;
                    result.relpath = std::move( normalPath );
                }
                else
                {
                    throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
                }

                pathOut = std::move( result );

                return true;
            }
        }

        return false;
    }

    // Common path transformation function.
    // So we do not have four-times repeated code everywhere.
    template <typename charType>
    AINLINE bool ParseTranslatorPath(
        const charType *path, eRequestedPathResolution outputType,
        const translatorPathResult& curDirPath, eBaseDirDesignation baseDirType, bool allowFile,
        normalNodePath& nodePathOut,
        filePath *descOut = nullptr, bool *slashDirOut = nullptr
    ) const
    {
        parsedTranslatorPath parsedPath = this->DispatchTranslatorPath( path, allowFile );

        // The default transformation function checks for outbreak based on the root descriptor.
        bool outbreakEnabled = this->m_enableOutbreak;

        // Cannot outbreak in relative output. It is common sense.
        if ( outbreakEnabled == false || outputType == eRequestedPathResolution::RELATIVE_PATH )
        {
            if ( this->HasRootDescriptorChanged( parsedPath ) )
            {
                return false;
            }
        }

        translatorPathResult transPath;

        bool success = this->OutputTranslatorPath(
            std::move( parsedPath ), outputType,
            curDirPath, baseDirType,
            transPath
        );

        if ( !success )
        {
            return false;
        }

        // Translate the result to normal path.
        normalNodePath normalPath;

        if ( outputType == eRequestedPathResolution::FULL_PATH )
        {
            normalPath.backCount = 0;
            normalPath.travelNodes = std::move( transPath.fullpath.RootNodes() );
            normalPath.isFilePath = transPath.fullpath.IsFilePath();
        }
        else if ( outputType == eRequestedPathResolution::RELATIVE_PATH )
        {
            normalPath = std::move( transPath.relpath );
        }
        else
        {
            throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
        }

        nodePathOut = std::move( normalPath );

        // Also return some meta-information, if desired.
        eRequestedPathResolution pathType = transPath.type;

        if ( descOut )
        {
            if ( pathType == eRequestedPathResolution::FULL_PATH )
            {
                *descOut = transPath.fullpath.RootDescriptor();
            }
            else
            {
                *descOut = this->m_rootPath.RootDescriptor();
            }
        }

        if ( slashDirOut )
        {
            if ( pathType == eRequestedPathResolution::FULL_PATH )
            {
                *slashDirOut = transPath.fullpath.DecideSlashDirection();
            }
            else
            {
                *slashDirOut = this->m_rootPath.DecideSlashDirection();
            }
        }

        return true;
    }

    template <typename charType>
    AINLINE bool ParseTranslatorPathGuided(
        const charType *path,
        const translatorPathResult& curDirPath, eBaseDirDesignation baseDirType, bool allowFile,
        translatorPathResult& pathOut
    ) const
    {
        parsedTranslatorPath parsedPath = this->DispatchTranslatorPath( path, allowFile );

        bool hasRootDescChanged = this->HasRootDescriptorChanged( parsedPath );

        eRequestedPathResolution outputType;

        if ( hasRootDescChanged )
        {
            if ( this->m_enableOutbreak == false )
            {
                return false;
            }

            outputType = eRequestedPathResolution::FULL_PATH;
        }
        else
        {
            outputType = eRequestedPathResolution::RELATIVE_PATH;
        }

        return this->OutputTranslatorPath(
            std::move( parsedPath ),
            outputType, this->m_curDirPath, eBaseDirDesignation::ROOT_DIR,
            pathOut
        );
    }

private:
    template <typename charType>
    bool            IntGetFullPathNodes             ( const charType *path, bool allowFile, normalNodePath& nodesOut, filePath *descOut = nullptr, bool *slashDirOut = nullptr ) const
    {
        try
        {
            normalNodePath nodePath;

            if ( !ParseTranslatorPath( path, eRequestedPathResolution::FULL_PATH, this->m_curDirPath, eBaseDirDesignation::CUR_DIR, allowFile, nodePath, descOut, slashDirOut ) )
            {
                return false;
            }

            assert( nodePath.backCount == 0 );

            nodesOut = std::move( nodePath );

            return true;
        }
        catch( eir::codepoint_exception& )
        {
            return false;
        }
        catch( FileSystem::filesystem_exception& except )
        {
            if ( except.get_code() == FileSystem::eGenExceptCode::ILLEGAL_PATHCHAR )
            {
                return false;
            }

            throw;
        }
    }
    template <typename charType>
    bool            IntGetFullPathNodesFromRoot     ( const charType *path, bool allowFile, normalNodePath& nodesOut, filePath *descOut = nullptr, bool *slashDirOut = nullptr ) const
    {
        try
        {
            // If the path is wrong in both invalid-character and invalid-breakout, then we make no guarantees
            // about which exception is raised first.

            // In this function we actually give no current-dir to the runtime.

            normalNodePath nodePath;

            if ( !ParseTranslatorPath( path, eRequestedPathResolution::FULL_PATH, translatorPathResult(), eBaseDirDesignation::ROOT_DIR, allowFile, nodePath, descOut, slashDirOut ) )
            {
                return false;
            }

            assert( nodePath.backCount == 0 );

            nodesOut = std::move( nodePath );

            return true;
        }
        catch( eir::codepoint_exception& )
        {
            return false;
        }
        catch( FileSystem::filesystem_exception& except )
        {
            if ( except.get_code() == FileSystem::eGenExceptCode::ILLEGAL_PATHCHAR )
            {
                return false;
            }

            throw;
        }
    }
    template <typename charType>
    bool            IntGetRelativePathNodes         ( const charType *path, bool allowFile, normalNodePath& nodesOut ) const
    {
        try
        {
            normalNodePath nodePath;

            if ( !ParseTranslatorPath( path, eRequestedPathResolution::RELATIVE_PATH, this->m_curDirPath, eBaseDirDesignation::CUR_DIR, allowFile, nodePath ) )
            {
                return false;
            }

            nodesOut = std::move( nodePath );

            return true;
        }
        catch( eir::codepoint_exception& )
        {
            return false;
        }
        catch( FileSystem::filesystem_exception& except )
        {
            if ( except.get_code() == FileSystem::eGenExceptCode::ILLEGAL_PATHCHAR )
            {
                return false;
            }

            throw;
        }
    }
    template <typename charType>
    bool            IntGetRelativePathNodesFromRoot ( const charType *path, bool allowFile, normalNodePath& nodesOut ) const
    {
        try
        {
            normalNodePath nodePath;

            if ( !ParseTranslatorPath( path, eRequestedPathResolution::RELATIVE_PATH, this->m_curDirPath, eBaseDirDesignation::ROOT_DIR, allowFile, nodePath ) )
            {
                return false;
            }

            nodesOut = std::move( nodePath );

            return true;
        }
        catch( eir::codepoint_exception& )
        {
            return false;
        }
        catch( FileSystem::filesystem_exception& except )
        {
            if ( except.get_code() == FileSystem::eGenExceptCode::ILLEGAL_PATHCHAR )
            {
                return false;
            }

            throw;
        }
    }

public:
    // Method redirects for the generic ones.
    bool            GetFullPathNodesFromRoot        ( const char *path, normalNodePath& nodes ) const override final                { return GenGetFullPathNodesFromRoot( path, nodes ); }
    bool            GetFullPathNodes                ( const char *path, normalNodePath& nodes ) const override final                { return GenGetFullPathNodes( path, nodes ); }
    bool            GetRelativePathNodesFromRoot    ( const char *path, normalNodePath& nodes ) const override final                { return GenGetRelativePathNodesFromRoot( path, nodes ); }
    bool            GetRelativePathNodes            ( const char *path, normalNodePath& nodes ) const override final                { return GenGetRelativePathNodes( path, nodes ); }
    bool            GetFullPathFromRoot             ( const char *path, bool allowFile, filePath& output ) const override final     { return GenGetFullPathFromRoot( path, allowFile, output ); }
    bool            GetFullPath                     ( const char *path, bool allowFile, filePath& output ) const override final     { return GenGetFullPath( path, allowFile, output ); }
    bool            GetRelativePathFromRoot         ( const char *path, bool allowFile, filePath& output ) const override final     { return GenGetRelativePathFromRoot( path, allowFile, output ); }
    bool            GetRelativePath                 ( const char *path, bool allowFile, filePath& output ) const override final     { return GenGetRelativePath( path, allowFile, output ); }
    bool            ChangeDirectory                 ( const char *path ) override final                                             { return GenChangeDirectory( path ); }

    bool            GetFullPathNodesFromRoot        ( const wchar_t *path, normalNodePath& nodes ) const override final             { return GenGetFullPathNodesFromRoot( path, nodes ); }
    bool            GetFullPathNodes                ( const wchar_t *path, normalNodePath& nodes ) const override final             { return GenGetFullPathNodes( path, nodes ); }
    bool            GetRelativePathNodesFromRoot    ( const wchar_t *path, normalNodePath& nodes ) const override final             { return GenGetRelativePathNodesFromRoot( path, nodes ); }
    bool            GetRelativePathNodes            ( const wchar_t *path, normalNodePath& nodes ) const override final             { return GenGetRelativePathNodes( path, nodes ); }
    bool            GetFullPathFromRoot             ( const wchar_t *path, bool allowFile, filePath& output ) const override final  { return GenGetFullPathFromRoot( path, allowFile, output ); }
    bool            GetFullPath                     ( const wchar_t *path, bool allowFile, filePath& output ) const override final  { return GenGetFullPath( path, allowFile, output ); }
    bool            GetRelativePathFromRoot         ( const wchar_t *path, bool allowFile, filePath& output ) const override final  { return GenGetRelativePathFromRoot( path, allowFile, output ); }
    bool            GetRelativePath                 ( const wchar_t *path, bool allowFile, filePath& output ) const override final  { return GenGetRelativePath( path, allowFile, output ); }
    bool            ChangeDirectory                 ( const wchar_t *path ) override final                                          { return GenChangeDirectory( path ); }

    bool            GetFullPathNodesFromRoot        ( const char8_t *path, normalNodePath& nodes ) const override final             { return GenGetFullPathNodesFromRoot( path, nodes ); }
    bool            GetFullPathNodes                ( const char8_t *path, normalNodePath& nodes ) const override final             { return GenGetFullPathNodes( path, nodes ); }
    bool            GetRelativePathNodesFromRoot    ( const char8_t *path, normalNodePath& nodes ) const override final             { return GenGetRelativePathNodesFromRoot( path, nodes ); }
    bool            GetRelativePathNodes            ( const char8_t *path, normalNodePath& nodes ) const override final             { return GenGetRelativePathNodes( path, nodes ); }
    bool            GetFullPathFromRoot             ( const char8_t *path, bool allowFile, filePath& output ) const override final  { return GenGetFullPathFromRoot( path, allowFile, output ); }
    bool            GetFullPath                     ( const char8_t *path, bool allowFile, filePath& output ) const override final  { return GenGetFullPath( path, allowFile, output ); }
    bool            GetRelativePathFromRoot         ( const char8_t *path, bool allowFile, filePath& output ) const override final  { return GenGetRelativePathFromRoot( path, allowFile, output ); }
    bool            GetRelativePath                 ( const char8_t *path, bool allowFile, filePath& output ) const override final  { return GenGetRelativePath( path, allowFile, output ); }
    bool            ChangeDirectory                 ( const char8_t *path ) override final                                          { return GenChangeDirectory( path ); }

    // Actual methods of this class.
    filePath        GetDirectory                    ( void ) const override final
    {
#ifdef FILESYS_MULTI_THREADING
        NativeExecutive::CReadWriteReadContextSafe <> pathConsist( this->lockPathConsistency );
#endif //FILESYS_MULTI_THREADING

        eRequestedPathResolution type = this->m_curDirPath.type;

        if ( type == eRequestedPathResolution::RELATIVE_PATH )
        {
            // We always want forward-slashes in relative paths.
            return _File_OutputNormalPath( this->m_curDirPath.relpath, true );
        }
        else if ( type == eRequestedPathResolution::FULL_PATH )
        {
            filePath result = this->m_curDirPath.fullpath.RootDescriptor();
            bool slashDir = this->m_curDirPath.fullpath.DecideSlashDirection();

            _File_OutputPathTree( this->m_curDirPath.fullpath.RootNodes(), false, slashDir, result );

            return result;
        }
        else
        {
            throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
        }
    }

    void            SetPathProcessMode      ( filesysPathProcessMode procMode ) override
    {
        this->m_pathProcessMode = procMode;
    }
    filesysPathProcessMode  GetPathProcessMode      ( void ) const noexcept override
    {
        return this->m_pathProcessMode;
    }

    bool            IsCaseSensitive                 ( void ) const final override       { return this->m_isCaseSensitive; }

    void            SetOutbreakEnabled              ( bool enabled )
    {
#ifdef FILESYS_MULTI_THREADING
        NativeExecutive::CReadWriteWriteContextSafe <> pathConsist( this->lockPathConsistency );
#endif //FILESYS_MULTI_THREADING

        this->m_enableOutbreak = enabled;
    }
    bool            IsOutbreakEnabled               ( void ) const final override
    {
#ifdef FILESYS_MULTI_THREADING
        NativeExecutive::CReadWriteReadContextSafe <> pathConsist( this->lockPathConsistency );
#endif //FILESYS_MULTI_THREADING

        return this->m_enableOutbreak;
    }

private:
    template <typename charType>
    inline bool IsTranslatorRootDescriptor( const charType *path, const charType*& pathAfterRootDesc ) const
    {
        typedef character_env <charType> char_env;

        try
        {
            // Get the UCP value in the beginning of said path.
            character_env_iterator_tozero <charType> desc_iter( path );

            typename char_env::ucp_t codepoint = desc_iter.ResolveAndIncrement();

            // Platform agnostic translator root descriptor.
            if ( codepoint == '/' && desc_iter.IsEnd() == false && desc_iter.Resolve() == '/' )
            {
                desc_iter.Increment();
                goto skipRootDesc;
            }

            if ( !m_isSystemPath )
            {
                // On Windows platforms, absolute paths are based on drive letters (C:/).
                // This means we can use the linux way of absolute fs root linking for the translators.
                // But this may confuse Linux users; on Linux this convention cannot work (reserved for abs paths).
                // Hence, usage of the '/' or '\\' descriptor is discouraged (only for backwards compatibility).
                // Disregard this thought for archive file systems, all file systems which are the root themselves.
                // They use the linux addressing convention.
                if ( codepoint == '/' )
                {
                    goto skipRootDesc;
                }
            }

            return false;

        skipRootDesc:
            pathAfterRootDesc = desc_iter.GetPointer();
            return true;
        }
        catch( eir::codepoint_exception& )
        {
            // Some very bad encoding situation happened.
            return false;
        }
    }

protected:
    friend class CFileSystem;
    friend struct CFileSystemNative;

    systemPathType          m_rootPath;         // absolute path to the translator root.
    translatorPathResult    m_curDirPath;       // path to current directory of translator using as little hoops as possible.

    // Called to confirm current directory change.
    virtual bool    OnConfirmDirectoryChange( const translatorPathResult& nodePath )
    {
        // Override this method to implement your own logic for changing directories.
        // You do not have to call back to this method.
        return true;
    }

private:
    bool            m_isSystemPath;
    bool            m_isCaseSensitive;
    bool            m_enableOutbreak;

protected:
    filesysPathProcessMode  m_pathProcessMode;

#ifdef FILESYS_MULTI_THREADING
    NativeExecutive::CReadWriteLock *lockPathConsistency;
#endif //FILESYS_MULTI_THREADING
};

// Often used by VFS translators.
template <typename pathCheckerType>
struct basicRootPathType
{
private:
    template <typename charType>
    static AINLINE bool IsAbsolutePath( const charType *path, const charType*& afterDescOut )
    {
        character_env_iterator_tozero <charType> iter( path );

        auto ucp = iter.ResolveAndIncrement();

        if ( FileSystem::IsDirectorySeparator( ucp ) )
        {
            afterDescOut = iter.GetPointer();
            return true;
        }

        return false;
    }

public:
    template <typename charType>
    AINLINE bool buildFromSystemPath( const charType *path, bool allowFile )
    {
        const charType *afterDesc;

        if ( IsAbsolutePath( path, afterDesc ) )
        {
            normalNodePath normalPath = _File_NormalizeRelativePath( afterDesc, pathCheckerType(), allowFile );

            if ( normalPath.backCount > 0 )
            {
                return false;
            }

            this->rootNodes = std::move( normalPath.travelNodes );
            this->isFilePath = normalPath.isFilePath;

            return true;
        }

        return false;
    }

    AINLINE void buildFromParams( const basicRootPathType& rootDesc, dirNames&& nodes, bool isFilePath )
    {
        this->rootNodes = std::move( nodes );
        this->isFilePath = isFilePath;
    }

    AINLINE dirNames& RootNodes( void )
    {
        return this->rootNodes;
    }
    AINLINE const dirNames& RootNodes( void ) const
    {
        return this->rootNodes;
    }
    AINLINE bool IsFilePath( void ) const
    {
        return this->isFilePath;
    }
    AINLINE filePath RootDescriptor( void ) const
    {
        return "/";
    }
    AINLINE bool doesRootDescriptorMatch( const basicRootPathType& ) const
    {
        return true;
    }
    AINLINE bool DecideSlashDirection( void ) const
    {
        // We always use forward-slashes.
        return true;
    }

    dirNames rootNodes;
    bool isFilePath;
};

#endif //_FILESYSTEM_TRANSLATOR_PATHUTIL_
