/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/OSUtils.vmemconf.h
*  PURPOSE:     Virtual-memory-based vector list of structs
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _NATIVE_OS_VERSIONING_HEADER_
#define _NATIVE_OS_VERSIONING_HEADER_

#if defined(__linux__)
#include <sys/utsname.h>
#include <string.h>
#include "NumericFormat.h"
#endif //OS DEPENDANT HEADERS

// Defines versioning information querying depending on operating system.
struct NativeOSVersionInfo
{
    inline NativeOSVersionInfo( void ) noexcept
    {
#ifdef __linux__
        struct utsname linux_version_info;

        uname( &linux_version_info );

        // Have to parse strings because the Linux kernel developers were out-of-their-minds
        // when thinking about application compatibility! Braindead people. Why design syscalls
        // that have flags depending on a specific version with an updatable kernel in mind, and
        // then make it such a hassle to get a version as an integer?
        const char *first_dot_or_end = &linux_version_info.release[0];
        bool first_is_dot = false;

        while ( first_dot_or_end < linux_version_info.release + countof(linux_version_info.release) )
        {
            if ( *first_dot_or_end == '\0' )
            {
                break;
            }

            if ( *first_dot_or_end == '.' )
            {
                first_is_dot = true;
                break;
            }

            first_dot_or_end++;
        }

        const char *second_num_beg = nullptr;
        const char *second_dot_or_end = nullptr;
        bool second_is_dot = false;

        if ( first_is_dot )
        {
            second_num_beg = first_dot_or_end + 1;
            second_dot_or_end = second_num_beg;

            while ( second_dot_or_end < linux_version_info.release + countof(linux_version_info.release) )
            {
                if ( *second_dot_or_end == '\0' )
                {
                    break;
                }

                if ( *second_dot_or_end == '.' )
                {
                    second_is_dot = true;
                    break;
                }

                second_dot_or_end++;
            }
        }

        size_t first_num_len = ( first_dot_or_end - linux_version_info.release );

        unsigned char linux_major = eir::to_number_len <unsigned char> ( linux_version_info.release, first_num_len );
        unsigned char linux_minor = 0;

        if ( second_num_beg != nullptr )
        {
            linux_minor = eir::to_number_len <unsigned char> ( second_num_beg, second_dot_or_end - second_num_beg );
        }

        this->linux_version_major = linux_major;
        this->linux_version_minor = linux_minor;

        // Attempt to detect any third party kernels that are known to cause issues.
        // API usage should be very careful in such kernels because they behave much stricter.
        bool is_third_party_kernel = false;

        if ( second_is_dot )
        {
            // Skip the revision number.
            const char *cur_parse = second_dot_or_end;
            unsigned int dash_count = 0;

            while ( cur_parse < linux_version_info.release + countof(linux_version_info.release) )
            {
                if ( *cur_parse == '\0' )
                {
                    break;
                }

                if ( *cur_parse == '-' )
                {
                    dash_count++;

                    if ( dash_count == 2 )
                    {
                        cur_parse++;
                        break;
                    }
                }

                cur_parse++;
            }

            if ( dash_count == 2 )
            {
                size_t remain_string_count = ( ( linux_version_info.release + countof(linux_version_info.release) ) - cur_parse );

                if ( strncmp( cur_parse, "Microsoft", remain_string_count ) == 0 )
                {
                    is_third_party_kernel = true;
                }
            }
        }

        this->is_third_party_kernel = is_third_party_kernel;
#endif //__linux__
    }
    inline NativeOSVersionInfo( const NativeOSVersionInfo& ) noexcept = default;
    inline NativeOSVersionInfo( NativeOSVersionInfo&& ) noexcept = default;

    inline NativeOSVersionInfo& operator = ( const NativeOSVersionInfo& ) noexcept = default;
    inline NativeOSVersionInfo& operator = ( NativeOSVersionInfo&& ) noexcept = default;

#ifdef __linux__
    inline unsigned char GetKernelMajor( void ) const noexcept
    {
        return this->linux_version_major;
    }

    inline unsigned char GetKernelMinor( void ) const noexcept
    {
        return this->linux_version_minor;
    }

    inline bool IsThirdPartyKernel( void ) const noexcept
    {
        return this->is_third_party_kernel;
    }
#endif //__linux__

private:
#ifdef __linux__
    unsigned char linux_version_major;
    unsigned char linux_version_minor;
    bool is_third_party_kernel;
#endif
};

#endif //_NATIVE_OS_VERSIONING_HEADER_
