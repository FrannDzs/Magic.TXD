/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/common/GlobPattern.h
*  PURPOSE:     glob-style pattern implementation
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _GLOB_PATTERN_HEADER_
#define _GLOB_PATTERN_HEADER_

#include "Exceptions.h"
#include "eirutils.h"
#include "MetaHelpers.h"
#include "UniChar.h"
#include "String.h"
#include "Vector.h"
#include "MultiString.h"

namespace eir
{

#define PPE_TEMPLARGS \
template <CharacterType charType, MemoryAllocator allocatorType, typename exceptMan = eir::DefaultExceptionManager>
#define PPE_TEMPLARGS_SUBANON \
template <CharacterType, MemoryAllocator, typename>

#define PPE_TEMPLUSE \
<charType, allocatorType, exceptMan>

PPE_TEMPLARGS
struct PathPatternEnv
{
private:
    // Makes templates friends of each other.
    PPE_TEMPLARGS_SUBANON friend struct PathPatternEnv;

    struct filePatternCommand_t
    {
        enum eCommand
        {
            FCMD_STRCMP,
            FCMD_WILDCARD,
            FCMD_REQONE,
            FCMD_STREND
        };

        eCommand cmd;
    };

    struct filePatternCommandCompare_t : public filePatternCommand_t
    {
        inline filePatternCommandCompare_t( void ) noexcept
        {
            this->cmd = filePatternCommand_t::FCMD_STRCMP;
        }

        size_t len;
    };

    struct filePatternCommandWildcard_t : public filePatternCommand_t
    {
        inline filePatternCommandWildcard_t( void ) noexcept
        {
            this->cmd = filePatternCommand_t::FCMD_WILDCARD;
        }

        size_t len;
    };

    // Allocation of a pattern command.
    template <typename commandType, typename... constrArgs>
    inline commandType* AllocateCommand( size_t memSize, constrArgs... args )
    {
        void *mem = this->data.opt().Allocate( this, memSize, alignof(commandType) );

        if ( !mem )
        {
            exceptMan::throw_oom( memSize );
        }

        try
        {
            return new (mem) commandType( std::forward <constrArgs> ( args )... );
        }
        catch( ... )
        {
            this->data.opt().Free( this, mem );

            throw;
        }
    }

    inline void FreeCommand( filePatternCommand_t *cmd ) noexcept
    {
        // We assume that there is nothing to free for commands other then their backed storage.
        // So just free the memory.
        this->data.opt().Free( this, cmd );
    }

    // Allocator for dynamic on-stack objects used within this pattern environment.
    struct patDynAlloc
    {
        inline patDynAlloc( PathPatternEnv *env )
        {
            this->env = env;
        }

        AINLINE void* Allocate( void *sysPtr, size_t memSize, size_t alignment ) noexcept
        {
            PathPatternEnv *env = this->env;

            return env->data.opt().Allocate( env, memSize, alignment );
        }

        AINLINE bool Resize( void *sysPtr, void *mem, size_t newSize ) noexcept
        {
            PathPatternEnv *env = this->env;

            return env->data.opt().Resize( env, mem, newSize );
        }

        AINLINE void Free( void *sysPtr, void *mem ) noexcept
        {
            PathPatternEnv *env = this->env;

            env->data.opt().Free( env, mem );
        }

        PathPatternEnv *env;
    };

public:
    inline PathPatternEnv( bool caseSensitive ) noexcept
    {
        this->data.caseSensitive = caseSensitive;
    }

    template <typename... allocatorArgs>
    inline PathPatternEnv( eir::constr_with_alloc, allocatorArgs&&... args )
        : data( std::tuple(), std::tuple( std::forward <allocatorArgs> ( args )... ) )
    {
        this->data.caseSensitive = false;
    }

    inline PathPatternEnv( const PathPatternEnv& ) = delete;

    template <MemoryAllocator otherAllocType, typename... otherPatternArgs>
        requires ( eir::nothrow_constructible_from <allocatorType, otherAllocType&&> )
    inline PathPatternEnv( PathPatternEnv <charType, otherAllocType, otherPatternArgs...>&& right ) noexcept
        : data( std::move( right.data.opt() ) )
    {
        this->data.caseSensitive = right.data.caseSensitive;

        right.data.caseSensitive = false;
    }

    inline ~PathPatternEnv( void )
    {
        return;
    }

    inline PathPatternEnv& operator = ( const PathPatternEnv& ) = delete;

    template <MemoryAllocator otherAllocType, typename... otherPatternArgs>
        requires ( std::is_nothrow_assignable <allocatorType&, otherAllocType&&>::value )
    inline PathPatternEnv& operator = ( PathPatternEnv <charType, otherAllocType, otherPatternArgs...>&& right ) noexcept
    {
        this->data.caseSensitive = right.data.caseSensitive;
        this->data.opt() = std::move( right.data.opt() );

        right.data.caseSensitive = false;

        return *this;
    }

    inline void SetCaseSensitive( bool caseSensitive ) noexcept
    {
        this->data.caseSensitive = caseSensitive;
    }

    inline bool IsCaseSensitive( void ) const noexcept
    {
        return this->data.caseSensitive;
    }

    struct filePattern_t
    {
        inline filePattern_t( PathPatternEnv *manager )
        {
            this->manager = manager;
        }

        inline filePattern_t( const filePattern_t& ) = delete;
        inline filePattern_t( filePattern_t&& right ) : commands( std::move( right.commands ) )
        {
            this->manager = right.manager;
        }

        inline ~filePattern_t( void )
        {
            PathPatternEnv *manager = this->manager;

            this->commands.Walk(
                [&]( size_t index, filePatternCommand_t *cmd )
            {
                manager->FreeCommand( cmd );
            });
        }

        inline filePattern_t& operator = ( const filePattern_t& ) = delete;
        inline filePattern_t& operator = ( filePattern_t&& right )
        {
            this->manager = right.manager;
            this->commands = std::move( right.commands );

            return *this;
        }

        inline bool IsEmpty( void ) const
        {
            return ( this->commands.GetCount() == 0 );
        }

    private:
        friend struct PathPatternEnv PPE_TEMPLUSE;

        DEFINE_HEAP_REDIR_ALLOC_BYSTRUCT( cmdRedirAlloc, allocatorType );

        PathPatternEnv *manager;
        typedef eir::Vector <filePatternCommand_t*, cmdRedirAlloc> cmdList_t;
        cmdList_t commands;
    };

private:
    inline filePatternCommand_t* CreatePatternCommand( typename filePatternCommand_t::eCommand cmd, const charType *token, size_t tokenLen )
    {
        switch( cmd )
        {
        case filePatternCommand_t::FCMD_STRCMP:
        {
            if ( tokenLen == 0 )
                return nullptr;

            size_t memSize = ( sizeof(filePatternCommandCompare_t) + sizeof(charType) * tokenLen );

            filePatternCommandCompare_t *cmd = AllocateCommand <filePatternCommandCompare_t> ( memSize );

            FSDataUtil::copy_impl( token, token + tokenLen, (charType*)( cmd + 1 ) );
            cmd->len = tokenLen;

            return cmd;
        }
        case filePatternCommand_t::FCMD_WILDCARD:
        {
            size_t memSize = ( sizeof(filePatternCommandWildcard_t) + sizeof(charType) * tokenLen );

            filePatternCommandWildcard_t *wildCmd = AllocateCommand <filePatternCommandWildcard_t> ( memSize );

            if ( tokenLen != 0 )
            {
                FSDataUtil::copy_impl( token, token + tokenLen, (charType*)( wildCmd + 1 ) );
            }
            wildCmd->len = tokenLen;

            return wildCmd;
        }
        case filePatternCommand_t::FCMD_REQONE:
        {
            size_t memSize = sizeof(filePatternCommand_t);

            filePatternCommand_t *cmd = AllocateCommand <filePatternCommand_t> ( memSize );

            cmd->cmd = filePatternCommand_t::FCMD_REQONE;
            return cmd;
        }
        case filePatternCommand_t::FCMD_STREND:
        {
            size_t memSize = sizeof(filePatternCommand_t);

            filePatternCommand_t *cmd = AllocateCommand <filePatternCommand_t> ( memSize );

            cmd->cmd = filePatternCommand_t::FCMD_STREND;
            return cmd;
        }
        }

        return nullptr;
    }

    inline void AddCommandToPattern( filePattern_t& pattern, typename filePatternCommand_t::eCommand pendCmd, const charType *token, size_t tokenLen )
    {
        if ( filePatternCommand_t *cmd = CreatePatternCommand( pendCmd, token, tokenLen ) )
        {
            pattern.commands.AddToBack( cmd );
        }
    }

public:
    template <typename inputCharType>
    inline filePattern_t CreatePattern( const inputCharType *buf )
    {
        typedef character_env <charType> store_env;
        typedef character_env <inputCharType> input_env;

        eir::String <charType, patDynAlloc, exceptMan> tokenBuf( eir::constr_with_alloc::DEFAULT, this );

        typename filePatternCommand_t::eCommand pendCmd = filePatternCommand_t::FCMD_STRCMP; // by default, it is string comparison.
        filePattern_t pattern( this );

        character_env_iterator_tozero <inputCharType> input_iter( buf );

        while ( !input_iter.IsEnd() )
        {
            const typename input_env::ucp_t code_pt = input_iter.ResolveAndIncrement();

            bool hasUsedBuf = false;

            if ( code_pt == L'*' )
            {
                AddCommandToPattern( pattern, pendCmd, tokenBuf.GetConstString(), tokenBuf.GetLength() );

                pendCmd = filePatternCommand_t::FCMD_WILDCARD;

                hasUsedBuf = true;
            }
            else if ( code_pt == L'?' )
            {
                AddCommandToPattern( pattern, pendCmd, tokenBuf.GetConstString(), tokenBuf.GetLength() );

                // Directly add our command
                AddCommandToPattern( pattern, filePatternCommand_t::FCMD_REQONE, nullptr, 0 ),

                // default back to string compare.
                pendCmd = filePatternCommand_t::FCMD_STRCMP;

                hasUsedBuf = true;
            }
            else
            {
                // Store the encoded character sequence.
                typename store_env::ucp_t enc_code_pt = AcquireDirectUCP <input_env, store_env> ( code_pt );

                typename store_env::enc_result enc_data;
                store_env::EncodeUCP( enc_code_pt, enc_data );

                for ( size_t n = 0; n < enc_data.numData; n++ )
                {
                    tokenBuf += enc_data.data[n];
                }
            }

            if ( hasUsedBuf )
            {
                tokenBuf.Clear();
            }
        }

        AddCommandToPattern( pattern, pendCmd, tokenBuf.GetConstString(), tokenBuf.GetLength() );

        if ( pendCmd != filePatternCommand_t::FCMD_WILDCARD || tokenBuf.GetLength() > 0 )
        {
            AddCommandToPattern( pattern, filePatternCommand_t::FCMD_STREND, nullptr, 0 );
        }

        return pattern;
    }

private:
    template <typename primCharType, typename secCharType>
    inline static bool CompareStrings_Count( const primCharType *primary, const secCharType *secondary, size_t count, bool case_insensitive )
    {
        return UniversalCompareStrings( primary, count, secondary, count, !case_insensitive );
    }

    template <typename inputCharType, typename cookieCharType>
    inline static const inputCharType* strnstr( const inputCharType *input, size_t input_len, const cookieCharType *cookie, size_t cookie_len, size_t& off_find, bool need_case_insensitive )
    {
        if ( input_len < cookie_len )
            return nullptr;

        if ( cookie_len == 0 )
        {
            off_find = 0;
            return input;
        }

        size_t scanEnd = ( input_len - cookie_len );

        size_t n = 0;

        character_env_iterator_tozero <inputCharType> iter( input );

        while ( n <= scanEnd )
        {
            if ( CompareStrings_Count( input + n, cookie, cookie_len, need_case_insensitive ) )
            {
                off_find = n;
                return input + n;
            }

            n += iter.GetIterateCount();

            iter.Increment();
        }

        return nullptr;
    }

public:
    template <typename secCharType>
    inline bool MatchPattern( const secCharType *input, const filePattern_t& pattern ) const
    {
        size_t input_len = cplen_tozero( input );

        bool need_case_insensitive = ( this->data.caseSensitive == false );

        size_t numCommands = pattern.commands.GetCount();

        for ( size_t n = 0; n < numCommands; n++ )
        {
            filePatternCommand_t *genCmd = pattern.commands[ n ];

            switch( genCmd->cmd )
            {
            case filePatternCommand_t::FCMD_STRCMP:
                {
                    const filePatternCommandCompare_t *cmpCmd = (const filePatternCommandCompare_t*)genCmd;
                    size_t len = cmpCmd->len;

                    if ( len > input_len )
                        return false;

                    const charType *cmpStr = (const charType*)( cmpCmd + 1 );

                    if ( CompareStrings_Count( input, cmpStr, len, need_case_insensitive ) == false )
                        return false;

                    input_len -= len;
                    input += len;
                }
                break;
            case filePatternCommand_t::FCMD_WILDCARD:
                {
                    const filePatternCommandWildcard_t *wildCmd = (const filePatternCommandWildcard_t*) genCmd;
                    const secCharType *nextPos;
                    size_t len = wildCmd->len;
                    size_t off_find;

                    const charType *findStr = (const charType*)( wildCmd + 1 );

                    if ( !( nextPos = strnstr( input, input_len, findStr, len, off_find, need_case_insensitive ) ) )
                        return false;

                    input_len -= ( off_find + len );
                    input = nextPos + len;
                }
                break;
            case filePatternCommand_t::FCMD_REQONE:
                {
                    if ( input_len == 0 )
                        return false;

                    charenv_charprov_tocplen char_prov( input, input_len );
                    size_t advCount = character_env_iterator <secCharType, decltype(char_prov)> ( input, std::move( char_prov ) ).GetIterateCount();

                    if ( input_len < advCount )
                        return false;

                    input_len -= advCount;
                    input += advCount;
                }
                break;
            case filePatternCommand_t::FCMD_STREND:
                if ( input_len > 0 )
                    return false;

                break;
            }
        }

        return true;
    }

    template <typename secCharType, typename patCharType>
    inline bool MatchPattern( const secCharType *input, const patCharType *patternString )
    {
        filePattern_t internal_pattern = this->CreatePattern( patternString );

        return this->MatchPattern( input, internal_pattern );
    }

    template <typename msAllocatorType>
    inline bool MatchPattern( const eir::MultiString <msAllocatorType>& input, const filePattern_t& pattern ) const
    {
        return input.char_dispatch(
            [&]( auto *input )
        {
            return MatchPattern( input, pattern );
        });
    }

private:
    struct fields
    {
        bool caseSensitive;
    };

    empty_opt <allocatorType, fields> data;
};

// Allocator optimizations.
PPE_TEMPLARGS
IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN PathPatternEnv PPE_TEMPLUSE::filePattern_t::cmdRedirAlloc::Allocate IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS
{
    filePattern_t *hostStruct = LIST_GETITEM( filePattern_t, refMem, commands );
    PathPatternEnv *env = hostStruct->manager;
    return env->data.opt().Allocate( env, memSize, alignment );
}
PPE_TEMPLARGS
IMPL_HEAP_REDIR_METH_RESIZE_RETURN PathPatternEnv PPE_TEMPLUSE::filePattern_t::cmdRedirAlloc::Resize IMPL_HEAP_REDIR_METH_RESIZE_ARGS(allocatorType)
{
    filePattern_t *hostStruct = LIST_GETITEM( filePattern_t, refMem, commands );
    PathPatternEnv *env = hostStruct->manager;
    return env->data.opt().Resize( env, objMem, reqNewSize );
}
PPE_TEMPLARGS
IMPL_HEAP_REDIR_METH_REALLOC_RETURN PathPatternEnv PPE_TEMPLUSE::filePattern_t::cmdRedirAlloc::Realloc IMPL_HEAP_REDIR_METH_REALLOC_ARGS(allocatorType)
{
    filePattern_t *hostStruct = LIST_GETITEM( filePattern_t, refMem, commands );
    PathPatternEnv *env = hostStruct->manager;
    return env->data.opt().Realloc( env, objMem, reqNewSize, alignment );
}
PPE_TEMPLARGS
IMPL_HEAP_REDIR_METH_FREE_RETURN PathPatternEnv PPE_TEMPLUSE::filePattern_t::cmdRedirAlloc::Free IMPL_HEAP_REDIR_METH_FREE_ARGS
{
    filePattern_t *hostStruct = LIST_GETITEM( filePattern_t, refMem, commands );
    PathPatternEnv *env = hostStruct->manager;
    env->data.opt().Free( env, memPtr );
}

} // namespace eir

#endif //_GLOB_PATTERN_HEADER_
