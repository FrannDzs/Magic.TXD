#pragma once

#define GLOB_REDIR( name )  \
    typedef decltype( &name ) name##_t; \
    name##_t name = NULL

#define GLOB_REDIR_L( module, name ) \
    this->name = (name##_t)GetProcAddress( module, #name )

// End.
