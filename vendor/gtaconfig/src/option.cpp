#include "StdInc.h"

#include "option.h"

OptionParser::OptionParser( const char *args[], size_t numArgs )
{
    this->args = args;
    this->numArgs = numArgs;

    this->curArg = 0;
    
    this->curArgPtr = UpdateArgPtr( 0, numArgs );
}

OptionParser::~OptionParser( void )
{
    return;
}

static const char OPTION_INITIALIZER = '-';

std::string OptionParser::FetchOption( void )
{
    size_t argIdx = this->curArg;
    size_t numArgs = this->numArgs;

    bool isParsingOption = false;
    std::string optString;

    const char *argPtr = this->curArgPtr;

    if ( argIdx < numArgs )
    {
        while ( true )
        {
            // Check if we can fetch from the current argument.
            char c = *argPtr;

            bool advanceIndex = false;

            if ( c == 0 )
            {
                // Need to advance to next argument.
                advanceIndex = true;
            }
            else
            {
                argPtr++;

                if ( c == OPTION_INITIALIZER )
                {
                    isParsingOption = true;
                }
                else if ( isParsingOption )
                {
                    if ( c == ' ' || c == '\t' )
                    {
                        // Terminate the option parsing.
                        advanceIndex = true;
                    }
                    else
                    {
                        optString += c;
                    }
                }
                else
                {
                    // Failure. No options.
                    argPtr--;
                    break;
                }
            }

            if ( advanceIndex )
            {
                argIdx++;
                argPtr = UpdateArgPtr( argIdx, numArgs );

                if ( isParsingOption || argIdx >= numArgs )
                {
                    break;
                }
            }
        }
    }

    this->curArg = argIdx;
    this->curArgPtr = argPtr;

    return optString;
}