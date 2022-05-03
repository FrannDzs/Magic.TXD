/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/texnameutils.hxx
*  PURPOSE:     Texture name validation utilities.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

struct texture_name_validator : public QValidator
{
    inline texture_name_validator( QObject *parent ) : QValidator( parent )
    {
        return;
    }

private:
    typedef character_env <wchar_t> wideEnv;
    typedef character_env_iterator_tozero <wchar_t> wideIter;

    static inline bool is_char_valid( wideEnv::ucp_t char_code )
    {
        return ( char_code >= 0x20 && char_code < 0x7F );
    }

public:
    void fixup( QString& input ) const override
    {
        // We only accept ANSI characters.
        std::wstring wideInput = input.toStdWString();

        // Make a new valid string.
        std::wstring validOut;

        wideIter iter( wideInput.c_str() );

        while ( !iter.IsEnd() )
        {
            wideEnv::ucp_t charCode = iter.ResolveAndIncrement();

            // Make sure we are valid ANSI.
            if ( !is_char_valid( charCode ) )
            {
                charCode = '?';
            }

            // Encode it into the wide string.
            wideEnv::enc_result wide_enc;
            wideEnv::EncodeUCP( charCode, wide_enc );

            for ( size_t n = 0; n < wide_enc.numData; n++ )
            {
                validOut += wide_enc.data[ n ];
            }
        }

        // Return the valid thing.
        input = QString::fromStdWString( validOut );
    }

    State validate( QString& str, int& cursor ) const override
    {
        // We validate this thing.
        std::wstring wideStr = str.toStdWString();

        wideIter iter( wideStr.c_str() );
        
        bool needs_fixup = false;

        while ( !iter.IsEnd() )
        {
            wideEnv::ucp_t charCode = iter.ResolveAndIncrement();

            if ( !is_char_valid( charCode ) )
            {
                needs_fixup = true;
                break;
            }
        }

        if ( needs_fixup )
        {
            fixup( str );
        }

        return Acceptable;
    }
};