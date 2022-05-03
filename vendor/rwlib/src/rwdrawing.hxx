/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwdrawing.hxx
*  PURPOSE:     Drawing environment header
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_DRAWING_INTERNALS_
#define _RENDERWARE_DRAWING_INTERNALS_

#include "rwstatesort.hxx"

namespace rw
{

// Template parameters for easier management.
struct deviceStateValue
{
    deviceStateValue( void )
    {
        // We have to initialize this parameter this way.
        // Later in the engine we have to set up these values properly
        // That is looping through all TSS and applying them to our
        // internal table.
        value = (rwDeviceValue_t)-1;
    }

    deviceStateValue( const rwDeviceValue_t& value )
    {
        this->value = value;
    }

    void operator = ( rwDeviceValue_t value )
    {
        this->value = value;
    }

    operator rwDeviceValue_t ( void ) const
    {
        return value;
    }

    operator rwDeviceValue_t& ( void )
    {
        return value;
    }

    rwDeviceValue_t value;
};

};

#endif //_RENDERWARE_DRAWING_INTERNALS_