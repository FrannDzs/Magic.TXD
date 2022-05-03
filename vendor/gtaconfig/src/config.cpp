#include "StdInc.h"

inline void ConformBuffer( char *outBuf, const char *inBuf, int *useCount, bool allowTermination, char **outBufPtr = NULL, const char **inBufPtr = NULL )
{
    for (;;)
    {
        char c = *inBuf;

        if ( !useCount )
        {
            if ( c == '\0' )
                break;
        }

        if ( allowTermination )
        {
            if ( c == '\n' || c == '\0' )
                break;
        }

        if ( c < ' ' || c == ',' )
            c = ' ';

        *outBuf = c;

        outBuf++;
        inBuf++;

        if ( useCount )
        {
            if ( --( *useCount ) <= 0 )
                break;
        }
    }

    if ( outBufPtr != NULL )
    {
        *outBufPtr = outBuf;
    }

    if ( inBufPtr != NULL )
    {
        *inBufPtr = inBuf;
    }
}

inline const char* GetConfigLineStartOffset( const char *buf )
{
    // Offset until we start at valid offset
    while ( true )
    {
        char c = *buf;
        
        if ( c == '\0' || c > ' ' )
            break;
        
        buf++;
    }

    return buf;
}

bool Config::GetConfigLine( CFile *stream, std::string& configLineOut )
{
    eir::String <char, ConfigHeapAllocator> configLine;

    bool getLine = FileSystem::FileGetString( stream, configLine );

    if ( getLine )
    {
        // We can hack here. I guess.
        char *cfgLineBuf = (char*)configLine.GetConstString();
        int cfgLineCount = (int)configLine.GetLength();

        if ( cfgLineCount > 0 )
        {
            ConformBuffer( cfgLineBuf, cfgLineBuf, &cfgLineCount, false );

            const char *cfgStart = GetConfigLineStartOffset( cfgLineBuf );

            configLineOut = std::string( cfgStart, configLine.GetLength() - ( cfgStart - cfgLineBuf ) );
        }
        else
        {
            configLineOut.clear();
        }
    }

    return getLine;
}