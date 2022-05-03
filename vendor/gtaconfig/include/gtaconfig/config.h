#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <CFileSystemInterface.h>

namespace Config
{
    bool GetConfigLine( CFile *stream, std::string& configLineOut );
};

#endif //_CONFIG_H_