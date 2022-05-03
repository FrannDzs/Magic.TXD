/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/include/CFileSystem.DiskManager.h
*  PURPOSE:     Local filesystem storage availability manager
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_DISK_MANAGER_HEADER_
#define _FILESYSTEM_DISK_MANAGER_HEADER_

#ifdef _WIN32
#include <sdk/Endian.h>
#endif //_WIN32

namespace FileSystem
{

enum class eDiskMediaType
{
    UNKNOWN,
    ROTATING_SPINDLE,
    SOLID_STATE
};

#ifdef _WIN32

struct win32_deviceGUID
{
    std::uint32_t Data1;
    std::uint16_t Data2;
    std::uint16_t Data3;
    std::uint16_t Data4;
    endian::big_endian <std::uint16_t> Data5;
    endian::big_endian <std::uint32_t> Data6;

    AINLINE bool operator == ( const win32_deviceGUID& ) const = default;

    static bool from_guid( const char *guid, win32_deviceGUID& guidOut );
    static bool from_guid( const wchar_t *guid, win32_deviceGUID& guidOut );
    fsStaticString <wchar_t> to_guid( void ) const;
};

#endif //_WIN32

struct mountPointDeviceInfo
{
#ifdef _WIN32
    win32_deviceGUID volume_guid;
#endif //_WIN32
};

struct mountPointInfo
{
    fsStaticString <wchar_t> mnt_path;
    bool isRemovable;
    fsStaticSet <eDiskMediaType> mediaTypes;
    uint64_t diskSpace;
    uint64_t freeSpace;
    mountPointDeviceInfo devInfo;
};

struct CLogicalDiskUpdateNotificationInterface
{
    virtual void OnDiskAttach( const wchar_t *primaryMountPoint ) = 0;
    virtual void OnDiskDetach( const wchar_t *primaryMountPoint ) = 0;
};

} // namespace FileSystem

#endif //_FILESYSTEM_DISK_MANAGER_HEADER_