/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.translator.scanutil.hxx
*  PURPOSE:     ScanDirectory utilities across translator implementations
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIR_FILESYSTEM_SCANDIR_UTILS_
#define _EIR_FILESYSTEM_SCANDIR_UTILS_

#include <sdk/GlobPattern.h>

// Helper for wildcard resolution.
template <typename charType>
inline const charType* _ResolveValidWildcard( const charType *wildcard )
{
    // If the user did not select any wildcard then pick the default.
    if ( wildcard == nullptr )
    {
        wildcard = FileSystem::GetAnyWildcardSelector <charType> ();
    }

    return wildcard;
}

template <typename iterator_type, typename pattern_env_type>
struct filtered_fsitem_iterator
{
    typedef typename iterator_type::info_data info_data;

    template <typename iterConstrArgType>
    AINLINE filtered_fsitem_iterator(
        iterConstrArgType constrArg,
        scanFilteringFlags flags,
        bool performTwoPhase
    )
        : iter( std::move( constrArg ) )
        , flags( std::move( flags ) )
        , doTwoPhase( performTwoPhase )
    {
        if ( performTwoPhase )
        {
            this->iterateFiles = true;
        }
    }

private:
    AINLINE void RewindInternalSeek( void )
    {
        if ( this->has_progressed_iter )
        {
            this->iter.Rewind();

            this->has_progressed_iter = false;
        }
    }

public:
    AINLINE void Rewind( void )
    {
        this->RewindInternalSeek();

        if ( this->doTwoPhase )
        {
            this->iterateFiles = true;
        }
    }

    static AINLINE bool MatchDirectory( const pattern_env_type& patternEnv, const typename pattern_env_type::filePattern_t& pattern, const info_data& info )
    {
        return _File_DoesDirectoryMatch( patternEnv, pattern, info.filename );
    }

    AINLINE bool Next(
        const pattern_env_type& patternEnv, const typename pattern_env_type::filePattern_t& pattern,
        info_data& infoOut
    )
    {
    tryNextItem:
        typename iterator_type::info_data info;

        if ( !iter.Next( info ) )
        {
            if ( this->doTwoPhase )
            {
                if ( this->iterateFiles == true )
                {
                    this->iterateFiles = false;

                    this->RewindInternalSeek();

                    goto tryNextItem;
                }
            }

            return false;
        }

        this->has_progressed_iter = true;

        // If we are a two phase iterator, then we have to check if we are the correct return type.
        if ( this->doTwoPhase )
        {
            if ( this->iterateFiles )
            {
                if ( info.isDirectory )
                {
                    goto tryNextItem;
                }
            }
            else
            {
                if ( info.isDirectory == false )
                {
                    goto tryNextItem;
                }
            }
        }

        // Exclude by easy properties.
        if ( this->flags.noSystem && info.attribs.isSystem )
        {
            goto tryNextItem;
        }
        if ( this->flags.noHidden && info.attribs.isHidden )
        {
            goto tryNextItem;
        }
        if ( this->flags.noTemporary && info.attribs.isTemporary )
        {
            goto tryNextItem;
        }
        if ( this->flags.noJunctionOrLink && info.attribs.isJunctionOrLink )
        {
            goto tryNextItem;
        }

        eFilesysItemType itemType = info.attribs.type;

        if ( this->flags.noFile && itemType == eFilesysItemType::FILE )
        {
            goto tryNextItem;
        }
        if ( this->flags.noDirectory && itemType == eFilesysItemType::DIRECTORY )
        {
            goto tryNextItem;
        }

        // See if we match the pattern.
        if ( info.isDirectory )
        {
            if ( _File_IgnoreDirectoryScanEntry( this->flags.noCurrentDirDesc, this->flags.noParentDirDesc, info.filename ) )
            {
                goto tryNextItem;
            }

            if ( this->flags.noPatternOnDirs == false )
            {
                if ( MatchDirectory( patternEnv, pattern, info ) == false )
                {
                    goto tryNextItem;
                }
            }
        }
        else
        {
            if ( patternEnv.MatchPattern( info.filename, pattern ) == false )
            {
                goto tryNextItem;
            }
        }

        infoOut = std::move( info );
        return true;
    }

private:
    iterator_type iter;
    scanFilteringFlags flags;
    bool doTwoPhase;
    bool iterateFiles = false;          // only used in two-phase
    bool has_progressed_iter = false;
};

template <typename ioCharacterType, typename dir_iterator_type>
struct CGenericDirectoryIterator : public CDirectoryIterator
{
    template <typename iterConstrArgType, typename wildcardCharType>
    inline CGenericDirectoryIterator( bool isCaseSensitive, iterConstrArgType constrArg, scanFilteringFlags flags, const wildcardCharType *wildcard )
        : patternEnv( eir::constr_with_alloc::DEFAULT )
        , pattern( patternEnv.CreatePattern( wildcard ) )
        , dirIter( std::move( constrArg ), std::move( flags ), false )
    {
        patternEnv.SetCaseSensitive( isCaseSensitive );
    }

    void Rewind( void )
    {
        this->dirIter.Rewind();
    }

    bool Next( item_info& infoOut )
    {
        typename dir_iterator_type::info_data plat_info;

        bool gotPlatInfo = this->dirIter.Next( this->patternEnv, this->pattern, plat_info );

        if ( !gotPlatInfo )
        {
            return false;
        }

        // Completely translate all entries to the generic container.
        item_info info;
        info.filename = std::move( plat_info.filename );
        info.isDirectory = plat_info.isDirectory;
        info.attribs = std::move( plat_info.attribs );

        infoOut = std::move( info );
        return true;
    }

private:
    eir::PathPatternEnv <ioCharacterType, FSObjectHeapAllocator> patternEnv;
    typename decltype(patternEnv)::filePattern_t pattern;
    filtered_fsitem_iterator <dir_iterator_type, decltype(patternEnv)> dirIter;
};

#endif //_EIR_FILESYSTEM_SCANDIR_UTILS_
