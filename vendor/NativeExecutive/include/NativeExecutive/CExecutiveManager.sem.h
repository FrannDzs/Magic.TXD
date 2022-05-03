/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.sem.internal.h
*  PURPOSE:     Semaphore object header
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATEXEC_SEMAPHORE_HEADER_
#define _NATEXEC_SEMAPHORE_HEADER_

BEGIN_NATIVE_EXECUTIVE

// Very simple semaphore that just supports one-increment and one-decrement.
// Any other semaphore type would dependend on the threading-subsystem so 
// no thanks.
struct CSemaphore
{
    // Insert another resource into us.
    void Increment( void );
    // Decrement a resource away from us and wait if no resource is present.
    void Decrement( void ) noexcept;
    // Try to decrement the semaphore, fail is no resource present without waiting.
    // Returns true if the decrement is successful, false otherwise.
    bool TryDecrement( void ) noexcept;
    // Try to take a resource but if the time elapsed then do not take it.
    // Returns true if the decrement was successful, false otherwise.
    bool TryTimedDecrement( unsigned int time_ms ) noexcept;
    // Debug function.
    size_t GetValue( void ) const noexcept;
};

END_NATIVE_EXECUTIVE

#endif //_NATEXEC_SEMAPHORE_HEADER_