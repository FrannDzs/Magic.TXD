#ifndef _OPTION_UTILITIES_
#define _OPTION_UTILITIES_

#include <string>

struct OptionParser
{
    OptionParser( const char *args[], size_t numArgs );
    ~OptionParser( void );

    std::string FetchOption( void );

    inline size_t GetArgIndex( void ) const             { return this->curArg; }
    inline const char* GetArgPointer( void ) const      { return this->curArgPtr; }

private:
    inline const char* UpdateArgPtr( size_t argIdx, size_t numArgs )
    {
        if ( argIdx < numArgs )
        {
            return this->args[ argIdx ];
        }
        else
        {
            return nullptr;
        }
    }

    // Management variables.
    const char **args;
    size_t numArgs;

    // Current iteration status.
    size_t curArg;
    const char *curArgPtr;
};

#endif //_OPTION_UTILITIES_