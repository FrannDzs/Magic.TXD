/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwlocalization.cpp
*  PURPOSE:     Translation support.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "pluginutil.hxx"

#include "rwfile.system.hxx"

#include <sdk/MemoryRaw.h>
#include <sdk/UniChar.h>

#include <sdk/StringUtils.h>

namespace rw
{

struct localizationEnv
{
    struct language
    {
        rwStaticMap <rwStaticString <wchar_t>, rwStaticString <wchar_t>, lexical_string_comparator <true>> valueMap;
        rwStaticString <wchar_t> key;
        rwStaticString <wchar_t> friendlyName;
        rwStaticString <wchar_t> author;
        rwStaticString <wchar_t> friendlyCountryName;
    };

    rwStaticMap <rwStaticString <wchar_t>, language, lexical_string_comparator <false>> languages;

    rwStaticString <wchar_t> current_language_key = L"en";

    template <typename ucp_t>
    static inline bool IsSpaceCharacter( ucp_t ucp )
    {
        return ( ucp == ' ' || ucp == '\t' );
    }

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif //__GNUC__

    static inline bool IsLineEnd( character_env <char8_t>::ucp_t ucp, character_env_iterator <char8_t, charenv_charprov_tocplen <char8_t>>& data_iter )
    {
        if ( ucp == '\n' )
        {
            return true;
        }
        if ( ucp == '\r' )
        {
            auto next_try_iter = data_iter;

            if ( next_try_iter.IsEnd() == false && next_try_iter.ResolveAndIncrement() == '\n' )
            {
                data_iter = next_try_iter;
                return true;
            }
        }

        return false;
    }

    static void parse_one_key_and_value( character_env_iterator <char8_t, charenv_charprov_tocplen <char8_t>>& data_iter, rwStaticString <wchar_t>& keyOut, rwStaticString <wchar_t>& valueOut )
    {
        // Skip any whitespace.
        character_env <char8_t>::ucp_t ucp;

        while ( !data_iter.IsEnd() && IsSpaceCharacter( ucp = data_iter.ResolveAndIncrement() ) )
        {}

        // If we did find a hashtag, we quit.
        if ( !data_iter.IsEnd() && ucp == '#' )
        {
            keyOut.Clear();
            valueOut.Clear();
            return;
        }

        // Next parse the key.
        rwStaticString <wchar_t> value, key;

        while ( !data_iter.IsEnd() )
        {
            if ( IsSpaceCharacter( ucp ) )
            {
                break;
            }
            if ( IsLineEnd( ucp, data_iter ) )
            {
                goto lineend;
            }

            CharacterUtil::TranscodeCharacter <char8_t, wchar_t> ( ucp, key );

            ucp = data_iter.ResolveAndIncrement();
        }

        // Skip any space characters between key and value.
        while ( !data_iter.IsEnd() && IsSpaceCharacter( ucp = data_iter.ResolveAndIncrement() ) )
        {}

        // Next parse the value.
        {
            size_t lastCountWithNonSpaceChar = 0;

            while ( !data_iter.IsEnd() )
            {
                if ( IsLineEnd( ucp, data_iter ) )
                {
                    break;
                }

                CharacterUtil::TranscodeCharacter <char8_t, wchar_t> ( ucp, value );

                // Keep track of how long the string has to be because we could always end up with a
                // non-space character later. Trim off trailing spaces.
                if ( IsSpaceCharacter( ucp ) == false )
                {
                    lastCountWithNonSpaceChar = value.GetLength();
                }

                ucp = data_iter.ResolveAndIncrement();
            }

            // Now do the trimming.
            value.Resize( lastCountWithNonSpaceChar );
        }

    lineend:
        keyOut = std::move( key );
        valueOut = std::move( value );
    }

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif //__GNUC__

    language load_language( const char8_t *data, size_t dataLen )
    {
        character_env_iterator <char8_t, charenv_charprov_tocplen <char8_t>> data_iter( data, { data, dataLen } );

        language result;

        // First parse properties, which are always at the start.
        while ( !data_iter.IsEnd() )
        {
            rwStaticString <wchar_t> key, value;
            parse_one_key_and_value( data_iter, key, value );

            if ( BoundedStringEqual( key.GetConstString(), key.GetLength(), "author", false ) )
            {
                result.author = std::move( value );
            }
            else if ( BoundedStringEqual( key.GetConstString(), key.GetLength(), "code", false ) )
            {
                result.key = std::move( value );
            }
            else if ( BoundedStringEqual( key.GetConstString(), key.GetLength(), "country", false ) )
            {
                result.friendlyCountryName = std::move( value );
            }
            else if ( BoundedStringEqual( key.GetConstString(), key.GetLength(), "name", false ) )
            {
                result.friendlyName = std::move( value );
            }
            else if ( key.IsEmpty() && value.IsEmpty() )
            {
                break;
            }
        }

        // Now parse language tokens.
        while ( !data_iter.IsEnd() )
        {
            rwStaticString <wchar_t> key, value;
            parse_one_key_and_value( data_iter, key, value );

            if ( key.IsEmpty() == false && value.IsEmpty() == false )
            {
                result.valueMap.Set( std::move( key ), std::move( value ) );
            }
        }

        return result;
    }

#ifndef RWLIB_RESINIT_DISABLE_LOCALIZATION
    void late_init( EngineInterface *engine )
    {
        // TODO: load all language files from the engine.
        FileTranslator::fileList_t language_files = fs::GetDirectoryListing( engine, L"languages/", L"*.lang" );

        if ( language_files.IsEmpty() )
        {
            // We shall try some special files aswell.
            language_files.Insert( L"languages/en.lang" );
            language_files.Insert( L"languages/de.lang" );
        }

        for ( auto filename : language_files )
        {
            StreamPtr langStream = fs::OpenDataStream( engine, filename.GetConstString(), RWSTREAMMODE_READONLY );

            if ( langStream.is_good() )
            {
                size_t streamSize = TRANSFORM_INT_CLAMP <size_t> ( langStream->size() );

                rwVector <char8_t> data( eir::constr_with_alloc::DEFAULT, engine );
                data.Resize( streamSize );

                langStream->read( data.GetData(), streamSize );

                try
                {
                    // Load the language.
                    language lang = this->load_language( data.GetData(), streamSize );

                    auto key = lang.key;

                    this->languages.Set( std::move( key ), std::move( lang ) );
                }
                catch( eir::codepoint_exception& )
                {
                    // Loading this language has apparently failed.
                    // Print a warning.
                    // Cannot look up language tokens because we possibly have zero languages loaded yet.
                    engine->PushWarningDynamic( L"failed to load language file due to UTF-8 error" );

                    // Continue on.
                }
            }
        }
    }

    static void wrapper_late_init( Interface *engine, void *ud )
    {
        localizationEnv *env = (localizationEnv*)ud;

        env->late_init( (EngineInterface*)engine );
    }
#endif //RWLIB_RESINIT_DISABLE_LOCALIZATION

    inline void Initialize( EngineInterface *rwEngine )
    {
#ifndef RWLIB_RESINIT_DISABLE_LOCALIZATION
        RegisterLateInitializer( rwEngine, wrapper_late_init, this, nullptr );
#endif //RWLIB_RESINIT_DISABLE_LOCALIZATION
    }

    inline void Shutdown( EngineInterface *rwEngine )
    {
        // Nothing really.
    }
};

static optional_struct_space <PluginDependantStructRegister <localizationEnv, RwInterfaceFactory_t>> localizationEnvRegister;

rwStaticString <wchar_t> GetLanguageItem( EngineInterface *rwEngine, const wchar_t *key )
{
    localizationEnv *locale = localizationEnvRegister.get().GetPluginStruct( rwEngine );

    if ( locale )
    {
        auto *foundLang = locale->languages.Find( locale->current_language_key );

        if ( foundLang )
        {
            auto *foundItem = foundLang->GetValue().valueMap.Find( key );

            if ( foundItem )
            {
                return foundItem->GetValue();
            }

            return key;
        }

        return L"invalid-current-language";
    }

    return L"locale-not-initialized";
}

void registerLocalizationEnvironment( void )
{
    localizationEnvRegister.Construct( engineFactory );
}

void unregisterLocalizationEnvironment( void )
{
    localizationEnvRegister.Destroy();
}

} // namespace rw
