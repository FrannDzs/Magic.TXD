/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/guiserialization.hxx
*  PURPOSE:     Internal header of editor serialization.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#define MAGICTXD_UNICODE_STRING_ID      0xBABE0001
#define MAGICTXD_CONFIG_BLOCK           0xBABE0002
#define MAGICTXD_ANSI_STRING_ID         0xBABE0003

enum magic_serializer_ids
{
    MAGICSERIALIZE_MAINWINDOW,
    MAGICSERIALIZE_MASSCONV,
    MAGICSERIALIZE_MASSEXPORT,
    MAGICSERIALIZE_EXPORTALLWINDOW,
    MAGICSERIALIZE_MASSBUILD,
    MAGICSERIALIZE_LANGUAGE,
    MAGICSERIALIZE_HELPERRUNTIME
};

// Our string blocks.
inline void RwWriteUnicodeString( rw::BlockProvider& prov, const rw::rwStaticString <wchar_t>& in )
{
    rw::BlockProvider stringBlock( &prov, MAGICTXD_UNICODE_STRING_ID, false );

    // Need to convert the string to a platform-fixed representation.
    auto utf16_format = CharacterUtil::ConvertStrings <wchar_t, char16_t> ( in );

    // Simply write stuff, without zero termination.
    stringBlock.write( utf16_format.GetConstString(), utf16_format.GetLength() * sizeof( char16_t ) );

    // Done.
}

inline rw::rwStaticString <wchar_t> RwReadUnicodeString( rw::BlockProvider& prov )
{
    rw::BlockProvider stringBlock( &prov, MAGICTXD_UNICODE_STRING_ID, false );

    // Will be reading a UTF16 string.
    rw::rwStaticString <char16_t> utf16_format;

    // Simply read stuff.
    rw::int64 blockLength = stringBlock.getBlockLength();

    // We need to get a valid unicode string length.
    size_t unicodeLength = ( blockLength / sizeof( char16_t ) );

    rw::int64 unicodeDataLength = ( unicodeLength * sizeof( char16_t ) );

    utf16_format.Resize( unicodeLength );

    // Read into the unicode string implementation.
    stringBlock.read( (char16_t*)utf16_format.GetConstString(), unicodeDataLength );

    // Skip the remainder.
    stringBlock.skip( blockLength - unicodeDataLength );

    return CharacterUtil::ConvertStrings <char16_t, wchar_t> ( utf16_format );
}

// ANSI string stuff.
inline void RwWriteANSIString( rw::BlockProvider& parentBlock, const rw::rwStaticString <char>& str )
{
    rw::BlockProvider stringBlock( &parentBlock, MAGICTXD_ANSI_STRING_ID );

    stringBlock.write( str.GetConstString(), str.GetLength() );
}

inline rw::rwStaticString <char> RwReadANSIString( rw::BlockProvider& parentBlock )
{
    rw::BlockProvider stringBlock( &parentBlock, MAGICTXD_ANSI_STRING_ID );

    // We read as much as we can into a memory buffer.
    rw::int64 blockSize = stringBlock.getBlockLength();

    size_t ansiStringLength = (size_t)blockSize;

    rw::rwStaticString <char> stringOut;

    stringOut.Resize( ansiStringLength );

    stringBlock.read( (void*)stringOut.GetConstString(), ansiStringLength );

    // Skip the rest.
    stringBlock.skip( blockSize - ansiStringLength );

    return stringOut;
}

#include <sdk/PluginHelpers.h>

// Serialization for MainWindow modules.
struct magicSerializationProvider abstract
{
    virtual void Load( MainWindow *mainWnd, rw::BlockProvider& configBlock ) = 0;
    virtual void Save( const MainWindow *mainWnd, rw::BlockProvider& configBlock ) const = 0;
};

rw::uint32 GetAmountOfMainWindowSerializers( const MainWindow *mainWnd );
magicSerializationProvider* FindMainWindowSerializer( MainWindow *mainWnd, unsigned short unique_id );
void ForAllMainWindowSerializers( const MainWindow *mainWnd, std::function <void ( magicSerializationProvider *prov, unsigned short id )> cb );

// API to register serialization.
bool RegisterMainWindowSerialization( MainWindow *mainWnd, unsigned short unique_id, magicSerializationProvider *prov );
bool UnregisterMainWindowSerialization( MainWindow *mainWnd, unsigned short unique_id );