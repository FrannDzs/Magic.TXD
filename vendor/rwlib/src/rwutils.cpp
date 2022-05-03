/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwutils.cpp
*  PURPOSE:     Utilities that do not depend on the internal state of the framework
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

namespace rw
{

namespace utils
{

void bufferedWarningManager::OnWarning( rwStaticString <wchar_t>&& msg )
{
    // We can take the string directly.
    this->messages.AddToBack( std::move( msg ) );
}

void bufferedWarningManager::forward( Interface *engineInterface )
{
    // Move away the warnings from us.
    auto warnings_to_push = std::move( this->messages );

    // We give all warnings to the engine.
    for ( size_t n = 0; n < warnings_to_push.GetCount(); n++ )
    {
        engineInterface->PushWarningDynamic( std::move( warnings_to_push[n] ) );
    }
}

// String chunk handling.
void writeStringChunkANSI( Interface *engineInterface, BlockProvider& outputProvider, const char *string, size_t strLen )
{
    // We are writing a string.
    BlockProvider stringChunk( &outputProvider, CHUNK_STRING );

    // Write the string.
    stringChunk.write(string, strLen);

    // Pad to multiples of four.
    // This will automatically zero-terminate the string.
    // We do this for performance reasons.
    size_t remainder = 4 - (strLen % 4);

    // Write zeroes.
    for ( size_t n = 0; n < remainder; n++ )
    {
        stringChunk.writeUInt8( 0 );
    }
}

void readStringChunkANSI( Interface *engineInterface, BlockProvider& inputProvider, rwStaticString <char>& stringOut )
{
    BlockProvider stringBlock( &inputProvider, CHUNK_STRING );

    int64 chunkLength = stringBlock.getBlockLength();

    if ( chunkLength < 0x80000000L )
    {
        size_t strLen = (size_t)chunkLength;

        rwStaticString <char> strRead;
        strRead.Resize( strLen );

        stringBlock.read( (char*)strRead.GetConstString(), strLen );

        stringOut = std::move( strRead );
    }
    else
    {
        engineInterface->PushWarningToken( L"RWUTIL_WARN_CHUNKSTRINGTOOLONG" );
    }
}

};

};