/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.DiskManager.cpp
*  PURPOSE:     Local filesystem storage availability manager
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

#include "CFileSystem.internal.h"

#include "CFileSystem.platform.h"

#include <sdk/PluginHelpers.h>
#include <sdk/UniChar.h>
#include <sdk/StringUtils.h>
#include <sdk/Set.h>
#include <sdk/NumericFormat.h>
#include <sdk/eirutils.h>

using namespace FileSystem;

namespace DeviceUtils
{

#ifdef _WIN32

static constexpr size_t MAX_VOLUME_GUIDPATH_LEN = 50u;

template <eir::CharacterType charType>
static inline bool _ExpectString( character_env_iterator_tozero <charType>& iter, const charType *str )
{
    character_env_iterator_tozero striter( str );

    const std::locale& classic_loc = std::locale::classic();

    toupper_lookup <character_env <charType>::ucp_t> facet( classic_loc );

    while ( true )
    {
        bool str_isEnd = striter.IsEnd();

        if ( str_isEnd )
        {
            break;
        }

        bool iter_isEnd = iter.IsEnd();

        if ( iter_isEnd != str_isEnd )
        {
            return false;
        }

        auto src_ucp = iter.ResolveAndIncrement();
        auto dst_ucp = striter.ResolveAndIncrement();

        if ( IsCharacterEqualEx( src_ucp, dst_ucp, facet, facet, false ) == false )
        {
            return false;
        }
    }

    return true;
}

template <eir::CharacterType charType>
static bool _ParseGUID( character_env_iterator_tozero <charType>& iter, win32_deviceGUID& guidOut )
{
    auto expect_nums = [&]( size_t numcnt )
    {
        size_t idx = 0;

        unsigned int tmp;

        while ( idx < numcnt && !iter.IsEnd() )
        {
            auto ucp = iter.ResolveAndIncrement();

            if ( eir::GetNumericForCharacter( ucp, tmp ) == false )
            {
                break;
            }

            idx++;
        }
    };

    const charType *beg_seg1 = iter.GetPointer();

    expect_nums( 8 );

    const charType *end_seg1 = iter.GetPointer();
    size_t seg1_cnt = ( end_seg1 - beg_seg1 );

    std::uint32_t seg1 = eir::to_number_len <std::uint32_t> ( beg_seg1, seg1_cnt, 16 );

    static const charType _minus_str[] =
    {
        eir::GetMinusSignCodepoint <charType> (), 0
    };

    if ( !_ExpectString( iter, _minus_str ) )
    {
        return false;
    }

    const charType *beg_seg2 = iter.GetPointer();

    expect_nums( 4 );

    const charType *end_seg2 = iter.GetPointer();
    size_t seg2_cnt = ( end_seg2 - beg_seg2 );

    std::uint16_t seg2 = eir::to_number_len <std::uint16_t> ( beg_seg2, seg2_cnt, 16 );

    if ( !_ExpectString( iter, _minus_str ) )
    {
        return false;
    }

    const charType *beg_seg3 = iter.GetPointer();

    expect_nums( 4 );

    const charType *end_seg3 = iter.GetPointer();
    size_t seg3_cnt = ( end_seg3 - beg_seg3 );

    std::uint16_t seg3 = eir::to_number_len <std::uint16_t> ( beg_seg3, seg3_cnt, 16 );

    if ( !_ExpectString( iter, _minus_str ) )
    {
        return false;
    }

    const charType *beg_seg4 = iter.GetPointer();

    expect_nums( 4 );

    const charType *end_seg4 = iter.GetPointer();
    size_t seg4_cnt = ( end_seg4 - beg_seg4 );

    std::uint16_t seg4 = eir::to_number_len <std::uint16_t> ( beg_seg4, seg4_cnt, 16 );

    if ( !_ExpectString( iter, _minus_str ) )
    {
        return false;
    }

    const charType *beg_seg5 = iter.GetPointer();

    expect_nums( 4 );

    const charType *end_seg5 = iter.GetPointer();
    size_t seg5_cnt = ( end_seg5 - beg_seg5 );

    std::uint16_t seg5 = eir::to_number_len <std::uint16_t> ( beg_seg5, seg5_cnt, 16 );

    const charType *beg_seg6 = end_seg5;

    expect_nums( 8 );

    const charType *end_seg6 = iter.GetPointer();
    size_t seg6_cnt = ( end_seg6 - beg_seg6 );

    std::uint32_t seg6 = eir::to_number_len <std::uint32_t> ( beg_seg6, seg6_cnt, 16 );

    win32_deviceGUID frmguid;
    frmguid.Data1 = seg1;
    frmguid.Data2 = seg2;
    frmguid.Data3 = seg3;
    frmguid.Data4 = seg4;
    frmguid.Data5 = seg5;
    frmguid.Data6 = seg6;

    guidOut = std::move( frmguid );
    return true;
}

static bool ParseVolumeGUIDPath( const wchar_t *guidpath, win32_deviceGUID& guidOut )
{
    character_env_iterator_tozero iter( guidpath );

    if ( !_ExpectString( iter, LR"(\\?\Volume{)" ) )
    {
        return false;
    }

    win32_deviceGUID frmguid;

    if ( !_ParseGUID( iter, frmguid ) )
    {
        return false;
    }

    if ( !_ExpectString( iter, LR"(}\)" ) )
    {
        return false;
    }

    if ( !iter.IsEnd() )
    {
        return false;
    }

    guidOut = std::move( frmguid );

    return true;
}

template <eir::MemoryAllocator allocatorType>
static eir::String <wchar_t, allocatorType> GenerateGUID( const win32_deviceGUID& frmguid )
{
    return eir::to_string_digitfill <wchar_t, allocatorType, 8> ( frmguid.Data1, 16 )
        + L'-'
        + eir::to_string_digitfill <wchar_t, allocatorType, 4> ( frmguid.Data2, 16 )
        + L'-'
        + eir::to_string_digitfill <wchar_t, allocatorType, 4> ( frmguid.Data3, 16 )
        + L'-'
        + eir::to_string_digitfill <wchar_t, allocatorType, 4> ( frmguid.Data4, 16 )
        + L'-'
        + eir::to_string_digitfill <wchar_t, allocatorType, 4> ( (std::uint16_t)frmguid.Data5, 16 )
        + eir::to_string_digitfill <wchar_t, allocatorType, 8> ( (std::uint32_t)frmguid.Data6, 16 );
}

template <eir::MemoryAllocator allocatorType>
static eir::String <wchar_t, allocatorType> GenerateVolumeGUIDDescriptor( const win32_deviceGUID& frmguid )
{
    return
        LR"(\\?\Volume{)"
        + GenerateGUID <allocatorType> ( frmguid )
        + LR"(})";
}

template <eir::MemoryAllocator allocatorType>
static eir::String <wchar_t, allocatorType> GenerateVolumeGUIDPath( const win32_deviceGUID& guid )
{
    return GenerateVolumeGUIDDescriptor <allocatorType> ( guid ) + L'\\';
}

#endif //_WIN32

} // namespace DeviceUtils

namespace FileSystem
{

#ifdef _WIN32

fsStaticString <wchar_t> win32_deviceGUID::to_guid( void ) const
{
    return DeviceUtils::GenerateGUID <FileSysCommonAllocator> ( *this );
}

bool win32_deviceGUID::from_guid( const char *guid, win32_deviceGUID& guidOut )
{
    character_env_iterator_tozero <char> iter( guid );

    return DeviceUtils::_ParseGUID( iter, guidOut );
}

bool win32_deviceGUID::from_guid( const wchar_t *guid, win32_deviceGUID& guidOut )
{
    character_env_iterator_tozero <wchar_t> iter( guid );

    return DeviceUtils::_ParseGUID( iter, guidOut );
}

#endif //_WIN32

} // namespace FileSystem

#ifdef _WIN32
#include "CFileSystem.native.win32.hxx"
#include <winioctl.h>
#endif //_WIN32

#ifdef FILESYS_MULTI_THREADING

#include "CFileSystem.lock.hxx"

#ifdef _WIN32
#include <Dbt.h>
#include <SetupAPI.h>
#elif defined(__linux__)
#include <string.h>
#include <poll.h>
#include <sys/statvfs.h>
#endif //_WIN32

static constinit optional_struct_space <fsLockProvider> diskManager_mountPointsLock;

struct filesysDiskManager : public NativeExecutive::threadActivityInterface
{
    AINLINE void BroadcastDiskArrival( const wchar_t *primMntPoint )
    {
        NativeExecutive::CReadWriteReadContext ctx_pushEvent( this->lock_notificatorList );

        for ( auto *intf : this->notificatorList )
        {
            try
            {
                intf->OnDiskAttach( primMntPoint );
            }
            catch( ... )
            {
                // Ignore any C++ exceptions.
            }
        }
    }

    AINLINE void BroadcastDiskDeparture( const wchar_t *primMntPoint )
    {
        NativeExecutive::CReadWriteReadContext ctx_pushEvent( this->lock_notificatorList );

        for ( auto *intf : this->notificatorList )
        {
            try
            {
                intf->OnDiskDetach( primMntPoint );
            }
            catch( ... )
            {
                // Ignore any C++ exceptions.
            }
        }
    }

#ifdef _WIN32
    static eir::eCompResult compare_guids( const win32_deviceGUID& left, const win32_deviceGUID& right ) noexcept
    {
        EIR_VALCMP( left.Data1, right.Data1 );
        EIR_VALCMP( left.Data2, right.Data2 );
        EIR_VALCMP( left.Data3, right.Data3 );

        const uint64_t& leftData4 = (const uint64_t&)left.Data4;
        const uint64_t& rightData4 = (const uint64_t&)right.Data4;

        EIR_VALCMP( leftData4, rightData4 );

        return eir::eCompResult::EQUAL;
    }

    struct registeredMountPoint
    {
        AINLINE registeredMountPoint( win32_deviceGUID guid, eir::Vector <eir::String <wchar_t, FSObjectHeapAllocator>, FSObjectHeapAllocator> mnt_paths ) noexcept
            : volume_guid( std::move( guid ) ), mnt_paths( std::move( mnt_paths ) )
        {
            return;
        }
        AINLINE registeredMountPoint( registeredMountPoint&& ) = default;
        AINLINE registeredMountPoint( const registeredMountPoint& ) = default;

        AINLINE registeredMountPoint& operator = ( registeredMountPoint&& ) = default;
        AINLINE registeredMountPoint& operator = ( const registeredMountPoint& ) = default;

        win32_deviceGUID volume_guid;
        eir::Vector <eir::String <wchar_t, FSObjectHeapAllocator>, FSObjectHeapAllocator> mnt_paths;
    };

    struct mountPointComparator
    {
        static AINLINE eir::eCompResult compare_values( const registeredMountPoint& left, const registeredMountPoint& right ) noexcept
        {
            return compare_guids( left.volume_guid, right.volume_guid );
        }
        static AINLINE eir::eCompResult compare_values( const registeredMountPoint& left, const win32_deviceGUID& right ) noexcept
        {
            return compare_guids( left.volume_guid, right );
        }
        static AINLINE eir::eCompResult compare_values( const win32_deviceGUID& left, const registeredMountPoint& right ) noexcept
        {
            return compare_guids( left, right.volume_guid );
        }
    };

    eir::Set <registeredMountPoint, FSObjectHeapAllocator, mountPointComparator> curr_mnt_points;

    void OnVolumeConfigurationChange( void )
    {
        // Update the mounts.
        decltype(curr_mnt_points) old_mounts;
        {
            // Need to reparse volumes and perform a global update.
            decltype(curr_mnt_points) curr_mounts = GetCurrentSystemMountPoints();

            NativeExecutive::CReadWriteWriteContext ctx_updatemounts( diskManager_mountPointsLock.get().GetReadWriteLock( this->get_file_system() ) );

            old_mounts = std::move( this->curr_mnt_points );

            this->curr_mnt_points = std::move( curr_mounts );
        }

        // Notify the system.
        old_mounts.ItemwiseComparison( this->curr_mnt_points,
            [&]( const registeredMountPoint& mnt, bool isLeft )
            {
                eir::String <wchar_t, FSObjectHeapAllocator> mntpath_tmp;
                const wchar_t *mntpath;

                if ( mnt.mnt_paths.GetCount() == 0 )
                {
                    mntpath_tmp = DeviceUtils::GenerateVolumeGUIDPath <FSObjectHeapAllocator> ( mnt.volume_guid );

                    mntpath = mntpath_tmp.GetConstString();
                }
                else
                {
                    mntpath = mnt.mnt_paths[ 0 ].GetConstString();
                }

                if ( isLeft )
                {
                    this->BroadcastDiskDeparture( mntpath );
                }
                else
                {
                    this->BroadcastDiskArrival( mntpath );
                }
            }
        );
    }

    struct deviceNotifyInfo
    {
        win32DevNotifyHandle devNotify;
        mutable eir::String <wchar_t, FSObjectHeapAllocator> device_name;
        mutable win32Handle currentDeviceHandle;
    };

    struct deviceNotifyInfoComparator
    {
        AINLINE static eir::eCompResult compare_values( const deviceNotifyInfo& left, const deviceNotifyInfo& right ) noexcept
        {
            return eir::DefaultValueCompare( left.devNotify, right.devNotify );
        }
        AINLINE static eir::eCompResult compare_values( const deviceNotifyInfo& left, HDEVNOTIFY right ) noexcept
        {
            return eir::DefaultValueCompare( left.devNotify, right );
        }
        AINLINE static eir::eCompResult compare_values( HDEVNOTIFY left, const deviceNotifyInfo& right ) noexcept
        {
            return eir::DefaultValueCompare( left, right.devNotify );
        }
    };

    eir::Set <deviceNotifyInfo, FSObjectHeapAllocator, deviceNotifyInfoComparator> active_device_notifications;

    static win32Handle CreateDeviceListeningHandle( const wchar_t *devpath )
    {
        win32_stacked_sysErrorMode noerrdlg( SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX );

        win32Handle hdev = CreateFileW( devpath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr );

        return hdev;
    }

    void AttemptRegisterDeviceNotificationForDevicePath( HWND wnd, eir::String <wchar_t, FSObjectHeapAllocator> devpath )
    {
        // We might be denied permission to the device or the device was already removed
        // at point of this call; handle this event properly.
        if ( win32Handle hdev = CreateDeviceListeningHandle( devpath.GetConstString() ) )
        {
            DEV_BROADCAST_HANDLE hinf;
            hinf.dbch_size = sizeof(hinf);
            hinf.dbch_devicetype = DBT_DEVTYP_HANDLE;
            hinf.dbch_reserved = 0;
            hinf.dbch_handle = hdev;        // handle will be stored by Win32.
            hinf.dbch_hdevnotify = nullptr; // handle will be stored by Win32.

            win32DevNotifyHandle devNotify = RegisterDeviceNotificationW( wnd, &hinf, DEVICE_NOTIFY_WINDOW_HANDLE );

            if ( devNotify )
            {
                // Try to register the handle.
                deviceNotifyInfo info;
                info.device_name = std::move( devpath );
                info.devNotify = std::move( devNotify );
                info.currentDeviceHandle = std::move( hdev );

                this->active_device_notifications.Insert( std::move( info ) );
            }
        }
    }

    static LRESULT CALLBACK NotificationWindowProcedure( HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam )
    {
        try
        {
            if ( msg == WM_CREATE )
            {
                const CREATESTRUCT *createargs = (const CREATESTRUCT*)lparam;

                SetWindowLongPtrW( wnd, GWLP_USERDATA, (LONG_PTR)createargs->lpCreateParams );

                return 1;
            }
            else if ( msg == WM_DEVICECHANGE )
            {
                filesysDiskManager *manager = (filesysDiskManager*)GetWindowLongPtrW( wnd, GWLP_USERDATA );

                if ( wparam == DBT_CUSTOMEVENT )
                {
                    const DEV_BROADCAST_HDR *info = (const DEV_BROADCAST_HDR*)lparam;

                    if ( info->dbch_devicetype == DBT_DEVTYP_HANDLE )
                    {
                        const DEV_BROADCAST_HANDLE *handleinfo = (const DEV_BROADCAST_HANDLE*)info;

                        // *** KNOWN EVENTS ***
                        static const GUID GUID_IO_CDROM_EXCLUSIVE_LOCK =
                        { 0xbc56c139, 0x7a10, 0x47ee, 0xa2, 0x94, 0x4c, 0x6a, 0x38, 0xf0, 0x14, 0x9a };
                        static const GUID GUID_IO_CDROM_EXCLUSIVE_UNLOCK =
                        { 0xa3b6d27d, 0x5e35, 0x4885, 0x81, 0xe5, 0xee, 0x18, 0xc0, 0x0e, 0xd7, 0x79 };
                        static const GUID GUID_IO_DEVICE_BECOMING_READY =
                        { 0xd07433f0, 0xa98e, 0x11d2, 0x91, 0x7a, 0x00, 0xa0, 0xc9, 0x06, 0x8f, 0xf3 };
                        static const GUID GUID_IO_DEVICE_EXTERNAL_REQUEST =
                        { 0xd07433d0, 0xa98e, 0x11d2, 0x91, 0x7a, 0x00, 0xa0, 0xc9, 0x06, 0x8f, 0xf3 };
                        static const GUID GUID_IO_MEDIA_ARRIVAL =
                        { 0xd07433c0, 0xa98e, 0x11d2, 0x91, 0x7a, 0x00, 0xa0, 0xc9, 0x06, 0x8f, 0xf3 };
                        static const GUID GUID_IO_MEDIA_EJECT_REQUEST =
                        { 0xd07433d1, 0xa98e, 0x11d2, 0x91, 0x7a, 0x00, 0xa0, 0xc9, 0x06, 0x8f, 0xf3 };
                        static const GUID GUID_IO_MEDIA_REMOVAL =
                        { 0xd07433c1, 0xa98e, 0x11d2, 0x91, 0x7a, 0x00, 0xa0, 0xc9, 0x06, 0x8f, 0xf3 };
                        static const GUID GUID_IO_VOLUME_CHANGE =
                        { 0x7373654a, 0x812a, 0x11d0, 0xbe, 0xc7, 0x08, 0x00, 0x2b, 0xe2, 0x09, 0x2f };
                        static const GUID GUID_IO_VOLUME_CHANGE_SIZE =
                        { 0x3a1625be, 0xad03, 0x49f1, 0x8e, 0xf8, 0x6b, 0xba, 0xc1, 0x82, 0xd1, 0xfd };
                        static const GUID GUID_IO_VOLUME_DISMOUNT =
                        { 0xd16a55e8, 0x1059, 0x11d2, 0x8f, 0xfd, 0x00, 0xa0, 0xc9, 0xa0, 0x6d, 0x32 };
                        static const GUID GUID_IO_VOLUME_DISMOUNT_FAILED =
                        { 0xe3c5b178, 0x105d, 0x11d2, 0x8f, 0xfd, 0x00, 0xa0, 0xc9, 0xa0, 0x6d, 0x32 };
                        static const GUID GUID_IO_VOLUME_FVE_STATUS_CHANGE =
                        { 0x062998b2, 0xee1f, 0x4b6a, 0xb8, 0x57, 0xe7, 0x6c, 0xbb, 0xe9, 0xa6, 0xda };
                        static const GUID GUID_IO_VOLUME_LOCK =
                        { 0x50708874, 0xc9af, 0x11d1, 0x8f, 0xef, 0x00, 0xa0, 0xc9, 0xa0, 0x6d, 0x32 };
                        static const GUID GUID_IO_VOLUME_LOCK_FAILED =
                        { 0xae2eed10, 0x0ba8, 0x11d2, 0x8f, 0xfb, 0x00, 0xa0, 0xc9, 0xa0, 0x6d, 0x32 };
                        static const GUID GUID_IO_VOLUME_MOUNT =
                        { 0xb5804878, 0x1a96, 0x11d2, 0x8f, 0xfd, 0x00, 0xa0, 0xc9, 0xa0, 0x6d, 0x32 };
                        static const GUID GUID_IO_VOLUME_NAME_CHANGE =
                        { 0x2de97f83, 0x4c06, 0x11d2, 0xa5, 0x32, 0x00, 0x60, 0x97, 0x13, 0x05, 0x5a };
                        static const GUID GUID_IO_VOLUME_NEED_CHKDSK =
                        { 0x799a0960, 0x0a0b, 0x4e03, 0xad, 0x88, 0x2f, 0xa7, 0xc6, 0xce, 0x74, 0x8a };
                        static const GUID GUID_IO_VOLUME_PHYSICAL_CONFIGURATION_CHANGE =
                        { 0x2de97f84, 0x4c06, 0x11d2, 0xa5, 0x32, 0x00, 0x60, 0x97, 0x13, 0x05, 0x5a };
                        static const GUID GUID_IO_VOLUME_PREPARING_EJECT =
                        { 0xc79eb16e, 0x0dac, 0x4e7a, 0xa8, 0x6c, 0xb2, 0x5c, 0xee, 0xaa, 0x88, 0xf6 };
                        static const GUID GUID_IO_VOLUME_UNIQUE_ID_CHANGE =
                        { 0xaf39da42, 0x6622, 0x41f5, 0x97, 0x0b, 0x13, 0x9d, 0x09, 0x2f, 0xa3, 0xd9 };
                        static const GUID GUID_IO_VOLUME_UNLOCK =
                        { 0x9a8c3d68, 0xd0cb, 0x11d1, 0x8f, 0xef, 0x00, 0xa0, 0xc9, 0xa0, 0x6d, 0x32 };
                        static const GUID GUID_IO_VOLUME_WEARING_OUT =
                        { 0x873113ca, 0x1486, 0x4508, 0x82, 0xac, 0xc3, 0xb2, 0xe5, 0x29, 0x7a, 0xaa };

                        GUID evtguid = handleinfo->dbch_eventguid;

                        if ( /* evtguid == GUID_IO_MEDIA_ARRIVAL || evtguid == GUID_IO_MEDIA_REMOVAL || */
                             evtguid == GUID_IO_VOLUME_MOUNT ||
                             evtguid == GUID_IO_VOLUME_UNIQUE_ID_CHANGE )
                        {
                            manager->OnVolumeConfigurationChange();
                        }
                    }
                }
                else if ( wparam == DBT_DEVICEARRIVAL || wparam == DBT_DEVICEREMOVECOMPLETE || wparam == DBT_DEVICEQUERYREMOVE || wparam == DBT_DEVICEQUERYREMOVEFAILED )
                {
                    const DEV_BROADCAST_HDR *info = (const DEV_BROADCAST_HDR*)lparam;

                    if ( info->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE )
                    {
                        const DEV_BROADCAST_DEVICEINTERFACE_W *intfinfo = (const DEV_BROADCAST_DEVICEINTERFACE_W*)info;

                        if ( wparam == DBT_DEVICEARRIVAL )
                        {
                            try
                            {
                                manager->AttemptRegisterDeviceNotificationForDevicePath( wnd, intfinfo->dbcc_name );
                            }
                            // We ignore exception throws in the device notification registration.
                            catch( ... )
                            {}

                            manager->OnVolumeConfigurationChange();
                        }
                        else if ( wparam == DBT_DEVICEREMOVECOMPLETE )
                        {
                            manager->OnVolumeConfigurationChange();
                        }
                    }
                    else if ( info->dbch_devicetype == DBT_DEVTYP_HANDLE )
                    {
                        const DEV_BROADCAST_HANDLE *handleinfo = (const DEV_BROADCAST_HANDLE*)info;

                        // Must not use the handle field inside handleinfo because it could be out of date by now.

                        HDEVNOTIFY devNotify = handleinfo->dbch_hdevnotify;

                        if ( auto *devInfoNode = manager->active_device_notifications.Find( devNotify ) )
                        {
                            auto& devInfo = devInfoNode->GetValue();

                            bool removeNode = false;

                            if ( wparam == DBT_DEVICEQUERYREMOVE )
                            {
                                devInfo.currentDeviceHandle = win32Handle();
                            }
                            else if ( wparam == DBT_DEVICEQUERYREMOVEFAILED )
                            {
                                win32Handle hdev = CreateDeviceListeningHandle( devInfo.device_name.GetConstString() );

                                if ( hdev )
                                {
                                    devInfo.currentDeviceHandle = std::move( hdev );
                                }
                                else
                                {
                                    removeNode = true;
                                }
                            }
                            else if ( wparam == DBT_DEVICEREMOVECOMPLETE )
                            {
                                removeNode = true;
                            }

                            if ( removeNode )
                            {
                                manager->active_device_notifications.RemoveNode( devInfoNode );
                            }
                        }
                    }
                }

                return 1;
            }
            else
            {
                return DefWindowProcW( wnd, msg, wparam, lparam );
            }
        }
        catch( ... )
        {
            // If even any C++ exception was thrown then we fail out of this handler.
            return 0;
        }
    }

    typedef eir::Vector <eir::String <wchar_t, FSObjectHeapAllocator>, FSObjectHeapAllocator> listOfVolumeMounts_t;

    static bool GetVolumeMountPoints( const wchar_t *volguidpath, listOfVolumeMounts_t& mountsOut )
    {
        listOfVolumeMounts_t results;

        eir::Vector <wchar_t, FSObjectHeapAllocator> result_concat;

        DWORD reallen = 0;

        while ( true )
        {
            if ( GetVolumePathNamesForVolumeNameW( volguidpath, result_concat.GetData(), reallen, &reallen ) == TRUE )
            {
                break;
            }

            DWORD lasterr = GetLastError();

            if ( lasterr != ERROR_MORE_DATA )
            {
                return false;
            }

            result_concat.Resize( reallen );
        }

        // Parse the string for the first mount point we encounter and return it.
        size_t iter = 0;
        size_t resultbuflen = result_concat.GetCount();

        const wchar_t *str_start = nullptr;

        auto commit_item = [&]( void )
        {
            if ( str_start != nullptr )
            {
                const wchar_t *str_end = ( result_concat.GetData() + iter );

                size_t str_len = ( str_end - str_start );

                if ( str_len > 0 )
                {
                    results.AddToBack( eir::String <wchar_t, FSObjectHeapAllocator> ( str_start, str_len ) );
                }

                str_start = nullptr;
            }
        };

        while ( iter < resultbuflen )
        {
            wchar_t nulpt = result_concat[ iter ];

            if ( nulpt == 0 )
            {
                commit_item();
            }
            else
            {
                if ( str_start == nullptr )
                {
                    str_start = result_concat.GetData();
                }
            }

            iter++;
        }

        commit_item();

        mountsOut = std::move( results );
        return true;
    }

    static decltype(curr_mnt_points) GetCurrentSystemMountPoints( void )
    {
        decltype(curr_mnt_points) mntpoints;

        wchar_t volguidpath[ DeviceUtils::MAX_VOLUME_GUIDPATH_LEN ];

        win32FindVolumeHandle findHandle = FindFirstVolumeW( volguidpath, countof(volguidpath) );

        if ( findHandle )
        {
            do
            {
                // Fetch the primary volume mount path.
                win32_deviceGUID volguid;
                listOfVolumeMounts_t mountPaths;

                if ( DeviceUtils::ParseVolumeGUIDPath( volguidpath, volguid ) && GetVolumeMountPoints( volguidpath, mountPaths ) )
                {
                    registeredMountPoint mntpt( volguid, std::move(mountPaths) );

                    mntpoints.Insert( std::move( mntpt ) );
                }
            }
            while ( FindNextVolumeW( findHandle, volguidpath, countof(volguidpath) ) );
        }

        return mntpoints;
    }

    ATOM classNotifyWnd;

    struct physicalDriveIterator
    {
        AINLINE physicalDriveIterator( HANDLE volumeHandle )
        {
            DWORD exts_bytesReturned;

            BOOL gotExtentInfo = DeviceIoControl(
                volumeHandle, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                nullptr, 0,
                &exts, sizeof( exts ),
                &exts_bytesReturned, nullptr
            );

            this->has_valid_extents = ( gotExtentInfo == TRUE && exts_bytesReturned >= sizeof(DWORD) );
            this->iter = 0;
        }
        AINLINE physicalDriveIterator( const physicalDriveIterator& ) = delete;
        AINLINE physicalDriveIterator( physicalDriveIterator&& ) = delete;

        AINLINE ~physicalDriveIterator( void ) = default;

        AINLINE physicalDriveIterator& operator = ( const physicalDriveIterator& ) = delete;
        AINLINE physicalDriveIterator& operator = ( physicalDriveIterator&& ) = delete;

        AINLINE bool IsEnd( void ) const
        {
            return ( this->has_valid_extents == false || this->iter == exts.NumberOfDiskExtents );
        }

        AINLINE win32Handle ResolveDriveHandle( void ) const
        {
            const DISK_EXTENT& extInfo = exts.Extents[ this->iter ];

            eir::String <wchar_t, FileSysCommonAllocator> physDescriptor( L"\\\\.\\PhysicalDrive" );

            physDescriptor += eir::to_string <wchar_t, FileSysCommonAllocator> ( extInfo.DiskNumber );

            return CreateFileW( physDescriptor.GetConstString(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr );
        }

        AINLINE void Increment( void )
        {
            this->iter++;
        }

    private:
        bool has_valid_extents;

        struct myVOLUME_DISK_EXTENTS
        {
            DWORD NumberOfDiskExtents;
            DISK_EXTENT Extents[64];
        };

        myVOLUME_DISK_EXTENTS exts;

        DWORD iter;
    };

    fsStaticSet <eDiskMediaType> Win32GetDiskMediaTypes( const wchar_t *volumedesc ) const
    {
        fsStaticSet <eDiskMediaType> mediaTypes;

        // Let's see what this disk is about.
        // There are characteristic features that drives have that define them.

        win32Handle volumeHandle = CreateFileW( volumedesc, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr );

        if ( volumeHandle == false )
            return mediaTypes;

        // We need Windows 7 and above for this check.
        if ( get_file_system()->m_win32HasLegacyPaths == false )
        {
            for ( physicalDriveIterator iter( volumeHandle ); !iter.IsEnd(); iter.Increment() )
            {
                win32Handle physhandle = iter.ResolveDriveHandle();

                if ( !physhandle )
                    continue;

                // Detect warm-up time. We think that devices without warm-up time are solid state.
                {
                    STORAGE_PROPERTY_QUERY query;
                    query.PropertyId = StorageDeviceSeekPenaltyProperty;
                    query.QueryType = PropertyStandardQuery;

                    DEVICE_SEEK_PENALTY_DESCRIPTOR seekPenalty;

                    DWORD query_bytesReturned;

                    BOOL gotParam = DeviceIoControl(
                        physhandle, IOCTL_STORAGE_QUERY_PROPERTY,
                        &query, sizeof( query ),
                        &seekPenalty, sizeof( seekPenalty ),
                        &query_bytesReturned, nullptr
                    );

                    if ( gotParam == TRUE &&
                         query_bytesReturned >= sizeof(DEVICE_SEEK_PENALTY_DESCRIPTOR) &&
                         seekPenalty.Version >= sizeof(DEVICE_SEEK_PENALTY_DESCRIPTOR) )
                    {
                        bool hasOverhead = ( seekPenalty.IncursSeekPenalty != FALSE );

                        if ( !hasOverhead )
                        {
                            // If we have no overhead, we definately are a flash-based device.
                            // Those devices tend to wear out faster than other, so lets call them "solid state".
                            mediaTypes.Insert( eDiskMediaType::SOLID_STATE );
                        }
                        else
                        {
                            // If we do have overhead, we consider it being a rotating thing, because those tend to be like that.
                            // Common sense says that rotating things are more reliable because otherwise nobody would want a rotating thing over a solid thing.
                            // Even if the setting-up-thing is cheaper, it does justify dumping temporary files on it.
                            mediaTypes.Insert( eDiskMediaType::ROTATING_SPINDLE );
                        }

                        continue;
                    }
                }

                // Check for TRIM command. I heard it is a good indicator for SSD.
                {
                    STORAGE_PROPERTY_QUERY query;
                    query.PropertyId = StorageDeviceTrimProperty;
                    query.QueryType = PropertyStandardQuery;

                    DEVICE_TRIM_DESCRIPTOR trimDesc;

                    DWORD trimDesc_bytesReturned;

                    BOOL gotParam = DeviceIoControl(
                        physhandle, IOCTL_STORAGE_QUERY_PROPERTY,
                        &query, sizeof(query),
                        &trimDesc, sizeof(trimDesc),
                        &trimDesc_bytesReturned, nullptr
                    );

                    if ( gotParam == TRUE &&
                         trimDesc_bytesReturned >= sizeof(trimDesc) &&
                         trimDesc.Version >= sizeof(trimDesc) )
                    {
                        // I heard that only solid state things support TRIM, so a good assumption?
                        bool supportsTRIM = ( trimDesc.TrimEnabled == TRUE );

                        if ( supportsTRIM )
                        {
                            mediaTypes.Insert( eDiskMediaType::SOLID_STATE );
                            continue;
                        }
                    }
                }
            }
        }

        return mediaTypes;
    }

    static bool Win32IsDiskRemovable( const wchar_t *volumedesc )
    {
        // Not sure if this makes any sense on Linux, because on there you can simply unmount things, so
        // even hard wired things count as removable!

        // Check things by volume handle.
        {
            win32Handle volumeHandle = CreateFileW( volumedesc, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr );

            if ( volumeHandle )
            {
                // Check the storage descriptor.
                {
                    STORAGE_PROPERTY_QUERY query;
                    query.PropertyId = StorageDeviceProperty;
                    query.QueryType = PropertyStandardQuery;

                    STORAGE_DEVICE_DESCRIPTOR devInfo;

                    DWORD devInfo_bytesReturned;

                    BOOL gotInfo = DeviceIoControl(
                        volumeHandle, IOCTL_STORAGE_QUERY_PROPERTY,
                        &query, sizeof( query ),
                        &devInfo, sizeof( devInfo ),
                        &devInfo_bytesReturned, nullptr
                    );

                    if ( gotInfo == TRUE &&
                         devInfo_bytesReturned >= sizeof(devInfo) &&
                         devInfo.Version >= sizeof(devInfo) )
                    {
                        // If this says that the drive is removable, sure it is!
                        bool isRemovable_desc = ( devInfo.RemovableMedia == TRUE );

                        if ( isRemovable_desc )
                        {
                            return true;
                        }
                    }
                }

                // Next check hot plug configuration.
                // This is a tricky check, as we iterate over every physical media attached to the volume.
                {
                    // Loop through all available.
                    for ( physicalDriveIterator driveIter( volumeHandle ); !driveIter.IsEnd(); driveIter.Increment() )
                    {
                        win32Handle physHandle = driveIter.ResolveDriveHandle();

                        if ( physHandle )
                        {
                            STORAGE_HOTPLUG_INFO hotplug_info;

                            DWORD hotplug_info_bytesReturned;

                            BOOL gotInfo = DeviceIoControl(
                                physHandle, IOCTL_STORAGE_GET_HOTPLUG_INFO,
                                nullptr, 0,
                                &hotplug_info, sizeof(hotplug_info),
                                &hotplug_info_bytesReturned, nullptr
                            );

                            if ( gotInfo == TRUE &&
                                 hotplug_info_bytesReturned >= sizeof(hotplug_info) &&
                                 hotplug_info.Size >= sizeof(hotplug_info) )
                            {
                                // If the device counts as hot-pluggable, we want to treat it as removable.
                                bool isHotplug = ( hotplug_info.MediaHotplug != 0 || hotplug_info.DeviceHotplug != 0 );
                                bool isRemovable = ( hotplug_info.MediaRemovable != 0 );

                                if ( isHotplug || isRemovable )
                                {
                                    // OK.
                                    return true;
                                }
                            }
                        }
                    }

                    // Alright.

                    // Apparently even the iteration through physical drives can lead to zero results.
                    // In that case, we have to try even harder...!
                }
            }
        }

        return false;
    }
#elif defined(__linux__)
    static AINLINE bool is_octal_num( char c )
    {
        return ( c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' );
    }

    struct registeredMountPoint
    {
        AINLINE registeredMountPoint( eir::String <char, FSObjectHeapAllocator> mnt_path ) noexcept
            : mnt_path( std::move( mnt_path ) )
        {
            return;
        }
        AINLINE registeredMountPoint( registeredMountPoint&& ) = default;
        AINLINE registeredMountPoint( const registeredMountPoint& ) = default;

        AINLINE registeredMountPoint& operator = ( registeredMountPoint&& ) = default;
        AINLINE registeredMountPoint& operator = ( const registeredMountPoint& ) = default;

        eir::String <char, FSObjectHeapAllocator> mnt_path;
    };

    struct mountPointComparator
    {
        static AINLINE eir::eCompResult compare_values( const registeredMountPoint& left, const registeredMountPoint& right ) noexcept
        {
            return lexical_string_comparator <true>::compare_values( left.mnt_path, right.mnt_path );
        }
    };

    eir::Set <registeredMountPoint, FSObjectHeapAllocator, mountPointComparator> curr_mnt_points;

    decltype(curr_mnt_points) parse_linux_mounts( int fd )
    {
        //CFileSystemNative *fileSys = this->get_file_system();

        decltype(curr_mnt_points) mnts;

        eir::Vector <char, FSObjectHeapAllocator> filebuf;

        char tmp[1024];

        while ( true )
        {
            ssize_t rdcnt = read( fd, tmp, sizeof(tmp) );

            if ( rdcnt <= 0 )
                break;

            size_t size_rdcnt = (size_t)rdcnt;

            size_t cur_data_seek = filebuf.GetCount();

            filebuf.Resize( cur_data_seek + size_rdcnt );

            memcpy( filebuf.GetData() + cur_data_seek, tmp, size_rdcnt );
        }

        size_t cur_coll_idx = 0;
        eir::String <char, FSObjectHeapAllocator> curtoken;

        decltype(registeredMountPoint::mnt_path) mnt_path;

        const char *mnts_data = filebuf.GetData();
        size_t mnts_len = filebuf.GetCount();

        charenv_charprov_tocplen <char> prov( mnts_data, mnts_len );
        character_env_iterator iter( mnts_data, std::move( prov ) );

        auto handle_token = [&]( void )
        {
            if ( cur_coll_idx == 1 )
            {
                mnt_path = std::move( curtoken );
            }
            else
            {
                curtoken.Clear();
            }
        };

        auto handle_new_reg = [&]( void )
        {
            size_t numcp = mnt_path.GetLength();

            if ( numcp > 0 )
            {
                char lastchar = mnt_path.GetConstString()[ numcp - 1 ];

                if ( lastchar != '/' )
                {
                    mnt_path += '/';
                }

                registeredMountPoint mntpt( std::move( mnt_path ) );

                mnts.Insert( std::move( mntpt ) );
            }
        };

        while ( iter.IsEnd() == false )
        {
            char c = iter.ResolveAndIncrement();

            if ( c == ' ' || c == '\r' )
            {
                if ( curtoken.IsEmpty() == false )
                {
                    handle_token();

                    cur_coll_idx++;
                }
            }
            else if ( c == '\n' )
            {
                if ( curtoken.IsEmpty() == false )
                {
                    handle_token();
                }

                handle_new_reg();

                cur_coll_idx = 0;
            }
            else
            {
                auto try_adv_iter = iter;

                if ( c == '\\' && try_adv_iter.ResolveAndIncrement() == '0' )
                {
                    const char *start_num = try_adv_iter.GetPointer();

                    while ( try_adv_iter.IsEnd() == false )
                    {
                        c = try_adv_iter.Resolve();

                        if ( is_octal_num( c ) == false )
                        {
                            break;
                        }

                        try_adv_iter.Increment();
                    }

                    const char *end_num = try_adv_iter.GetPointer();

                    iter = try_adv_iter;

                    size_t cnt = ( end_num - start_num );

                    c = eir::to_number_len <char> ( start_num, cnt, 8 );

                    curtoken += c;
                }
                else
                {
                    curtoken += c;
                }
            }
        }

        if ( curtoken.IsEmpty() == false )
        {
            handle_token();
        }

        handle_new_reg();

        return mnts;
    }
#endif //CROSS PLATFORM CODE

    CFileSystemNative* get_file_system( void );
    const CFileSystemNative* get_file_system( void ) const;

    filesysDiskManager& operator = ( const filesysDiskManager& ) = delete;

    NativeExecutive::CExecThread *notifyWndThread;

    NativeExecutive::CReadWriteLock *lock_notificatorList;
    eir::Vector <CLogicalDiskUpdateNotificationInterface*, FSObjectHeapAllocator> notificatorList;

#ifdef _WIN32
    static bool SetupDiGetFullDevicePathForInterfaceDetail( HDEVINFO devInfoSet, PSP_DEVICE_INTERFACE_DATA devData, eir::String <wchar_t, FSObjectHeapAllocator>& strOut )
    {
        DWORD reqlen = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        eir::Vector <char, FSObjectHeapAllocator> buf;

        buf.Resize( reqlen );
        {
            SP_DEVICE_INTERFACE_DETAIL_DATA_W *detailData = (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)buf.GetData();
            detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        }

        while ( true )
        {
            BOOL success = SetupDiGetDeviceInterfaceDetailW( devInfoSet, devData, (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)buf.GetData(), reqlen, &reqlen, nullptr );

            if ( success == TRUE )
            {
                break;
            }

            DWORD lasterr = GetLastError();

            if ( lasterr != ERROR_INSUFFICIENT_BUFFER )
            {
                return false;
            }

            buf.Resize( reqlen );
        }

        if ( reqlen < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W::cbSize) )
        {
            return false;
        }

        SP_DEVICE_INTERFACE_DETAIL_DATA_W *detailData = (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)buf.GetData();

        size_t strLen = 0;
        size_t maxcpcount = ( reqlen - sizeof(detailData->cbSize) );

        while ( strLen < maxcpcount )
        {
            wchar_t nulpt = detailData->DevicePath[ strLen ];

            if ( nulpt == 0 )
            {
                break;
            }

            strLen++;
        }

        strOut = eir::String <wchar_t, FSObjectHeapAllocator> ( detailData->DevicePath, strLen );
        return true;
    }
#endif //_WIN32

    static void _notify_thread_runtime( NativeExecutive::CExecThread *thread, void *ud )
    {
        NativeExecutive::CExecutiveManager *execMan = thread->GetManager();

        filesysDiskManager *diskMan = (filesysDiskManager*)ud;

#ifdef _WIN32
        win32WindowHandle notifyWnd = CreateWindowW(
            (LPCWSTR)MAKEINTATOM( diskMan->classNotifyWnd ),
            L"FileSystem_DiskNotify", 0, 0, 0, 0, 0, nullptr, nullptr,
            GetModuleHandleA( nullptr ), diskMan
        );
        win32DevNotifyHandle devNotify;

        static const GUID GUID_DEVINTERFACE_VOLUME =
        {
            0x53F5630D,
            0xB6BF,
            0x11D0,
            0x94, 0xF2, 0x00, 0xA0, 0xC9, 0x1E, 0xFB, 0x8B
        };

        if ( notifyWnd )
        {
            DEV_BROADCAST_DEVICEINTERFACE_W brd;
            brd.dbcc_size = sizeof(brd);
            brd.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
            brd.dbcc_reserved = 0;
            brd.dbcc_classguid = GUID_DEVINTERFACE_VOLUME;
            brd.dbcc_name[0] = 0;

            devNotify = RegisterDeviceNotificationW( notifyWnd, &brd, DEVICE_NOTIFY_WINDOW_HANDLE );
        }

        // Register all devices for listening on the system that do provide volume interfaces
        {
            HDEVINFO devInfoSet = SetupDiGetClassDevsW( &GUID_DEVINTERFACE_VOLUME, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE );

            if ( devInfoSet != INVALID_HANDLE_VALUE )
            {
                DWORD iter = 0;
                SP_DEVICE_INTERFACE_DATA intf_data;
                intf_data.cbSize = sizeof(intf_data);
                intf_data.Flags = 0;
                intf_data.Reserved = 0;

                while ( SetupDiEnumDeviceInterfaces( devInfoSet, nullptr, &GUID_DEVINTERFACE_VOLUME, iter++, &intf_data ) == TRUE )
                {
                    try
                    {
                        // Parse the device path from the returned device over here.
                        eir::String <wchar_t, FSObjectHeapAllocator> devpath;

                        if ( SetupDiGetFullDevicePathForInterfaceDetail( devInfoSet, &intf_data, devpath ) )
                        {
                            diskMan->AttemptRegisterDeviceNotificationForDevicePath( notifyWnd, devpath );
                        }
                    }
                    // Ignore any exceptions during device registration.
                    catch( ... )
                    {}
                }

                SetupDiDestroyDeviceInfoList( devInfoSet );
            }
        }

        // Start the listener task for mounting events.
        if ( notifyWnd && devNotify )
        {
            MSG msg;

            struct messageLoopHazardPreventInterface : public NativeExecutive::hazardPreventionInterface
            {
                HWND notifyWnd;

                void TerminateHazard( void ) noexcept override
                {
                    BOOL res = PostMessageW( this->notifyWnd, WM_QUIT, 0, 0 );

#ifdef _DEBUG
                    assert( res == TRUE );
#endif //_DEBUG
                }
            };
            messageLoopHazardPreventInterface intf;

            intf.notifyWnd = notifyWnd;

            NativeExecutive::hazardousSituation situation( execMan, &intf );

            while ( GetMessageW( &msg, notifyWnd, 0, 0 ) == TRUE )
            {
                execMan->CheckHazardCondition();

                TranslateMessage( &msg );
                DispatchMessageW( &msg );
            }
        }
#elif defined(__linux__)
        // Attempt to open the mounts table.
        int procMountsFD = open( "/proc/mounts", O_RDONLY );

        if ( procMountsFD >= 0 )
        {
            try
            {
                // Read the current mounts state.
                {
                    NativeExecutive::CReadWriteWriteContext ctx_initmnts( diskManager_mountPointsLock.get().GetReadWriteLock( diskMan->get_file_system() ) );

                    diskMan->curr_mnt_points = diskMan->parse_linux_mounts( procMountsFD );
                }

                // We create a special FD just for closing.
                int pipefds[2];

                int pipesucc = pipe( pipefds );

                if ( pipesucc >= 0 )
                {
                    try
                    {
                        struct pollingHazardSituation : public NativeExecutive::hazardPreventionInterface
                        {
                            void TerminateHazard( void ) noexcept
                            {
                                ssize_t writesucc = write( pipeinfd, "0", 1 );
                                (void)writesucc;
                            }

                            int pipeinfd;
                        };
                        pollingHazardSituation pollhaz;
                        pollhaz.pipeinfd = pipefds[1];

                        NativeExecutive::hazardousSituation situation( execMan, &pollhaz );

                        // Wait for changes in the mounts description.
                        pollfd cfgfds[2];
                        cfgfds[0].fd = procMountsFD;
                        cfgfds[0].events = POLLERR;
                        cfgfds[0].revents = 0;
                        cfgfds[1].fd = pipefds[0];
                        cfgfds[1].events = POLLIN;
                        cfgfds[1].revents = 0;

                        while ( true )
                        {
                            int pollerr = poll( cfgfds, countof(cfgfds), -1 );

                            int revents_main = cfgfds[0].revents;
                            cfgfds[0].revents = 0;

                            int revents_closing = cfgfds[1].revents;
                            cfgfds[1].revents = 0;

                            if ( pollerr < 0 || revents_closing != 0 )
                            {
                                break;
                            }
                            if ( pollerr == 0 )
                            {
                                continue;
                            }

                            if ( ( revents_main & POLLERR ) != 0 )
                            {
                                // Reset the seek to read the entire structure from the kernel.
                                off_t resOff = lseek( procMountsFD, 0, SEEK_SET );

                                assert( resOff == 0 );

                                // Read the new description of the mount points.
                                decltype(diskMan->curr_mnt_points) old_mounts;
                                {
                                    auto new_mounts = diskMan->parse_linux_mounts( procMountsFD );

                                    NativeExecutive::CReadWriteWriteContext ctx_updatemnts( diskManager_mountPointsLock.get().GetReadWriteLock( diskMan->get_file_system() ) );

                                    old_mounts = std::move( diskMan->curr_mnt_points );

                                    diskMan->curr_mnt_points = std::move( new_mounts );
                                }

                                // Parse the structure for changes.
                                // We basically perform an update.
                                old_mounts.ItemwiseComparison( diskMan->curr_mnt_points,
                                    [&]( auto mntpt, bool isOldMntPoint )
                                {
                                    auto wide_mnt_path = CharacterUtil::ConvertStrings <char, wchar_t> ( mntpt.mnt_path );

                                    if ( isOldMntPoint )
                                    {
                                        diskMan->BroadcastDiskDeparture( wide_mnt_path.GetConstString() );
                                    }
                                    else
                                    {
                                        diskMan->BroadcastDiskArrival( wide_mnt_path.GetConstString() );
                                    }
                                });
                            }
                        }
                    }
                    catch( ... )
                    {
                        close( pipefds[0] );
                        close( pipefds[1] );

                        throw;
                    }

                    close( pipefds[0] );
                    close( pipefds[1] );
                }
            }
            catch( ... )
            {
                close( procMountsFD );

                throw;
            }

            close( procMountsFD );
        }
#endif //CROSS PLATFORM CODE
    }

    fsStaticVector <mountPointInfo> GetCurrentMountPoints( void ) const
    {
        fsStaticVector <mountPointInfo> result;

        NativeExecutive::CReadWriteReadContextSafe ctx_readmounts( diskManager_mountPointsLock.get().GetReadWriteLock( this->get_file_system() ) );

#ifdef _WIN32
        // Make sure that the user does not get a weird error dialog spawned
        // that would halt the system.
        // NOTE: this stays even for the CreateTranslator calls down along so that those do not spawn
        // the dialogs either.
        win32_stacked_sysErrorMode noerrdlg( SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX );

        for ( auto& mntinfo : this->curr_mnt_points )
        {
            mountPointInfo info;

            auto voldesc = DeviceUtils::GenerateVolumeGUIDDescriptor <FileSysCommonAllocator> ( mntinfo.volume_guid );
            auto volumeguidpath = ( voldesc + L'\\' );

            if ( mntinfo.mnt_paths.GetCount() == 0 )
            {
                info.mnt_path = volumeguidpath;
            }
            else
            {
                info.mnt_path = mntinfo.mnt_paths[ 0 ];
            }

            info.isRemovable = Win32IsDiskRemovable( voldesc.GetConstString() );

            UINT diskMediaType = GetDriveTypeW( volumeguidpath.GetConstString() );

            if ( diskMediaType == DRIVE_CDROM || diskMediaType == DRIVE_CDROM )
            {
                info.mediaTypes.Insert( eDiskMediaType::ROTATING_SPINDLE );
            }
            else
            {
                info.mediaTypes = Win32GetDiskMediaTypes( voldesc.GetConstString() );
            }

            uint64_t freeSpaceNum = 0;
            uint64_t totalSpaceNum = 0;
            {
                DWORD sectorsPerCluster;
                DWORD sectorSize;
                DWORD numFreeClusters;
                DWORD numTotalClusters;

                BOOL gotFreeSpace = GetDiskFreeSpaceW( volumeguidpath.GetConstString(), &sectorsPerCluster, &sectorSize, &numFreeClusters, &numTotalClusters );

                if ( gotFreeSpace == TRUE )
                {
                    freeSpaceNum = ( (uint64_t)numFreeClusters * sectorsPerCluster * sectorSize );
                    totalSpaceNum = ( (uint64_t)numTotalClusters * sectorsPerCluster * sectorSize );
                }
            }

            info.freeSpace = freeSpaceNum;
            info.diskSpace = totalSpaceNum;
            info.devInfo.volume_guid = mntinfo.volume_guid;

            result.AddToBack( std::move( info ) );
        }
#elif defined(__linux__)
        for ( auto& mntinfo : this->curr_mnt_points )
        {
            mountPointInfo info;

            info.mnt_path = CharacterUtil::ConvertStrings <char, wchar_t, FileSysCommonAllocator> ( mntinfo.mnt_path.GetConstString() );
            info.isRemovable = true;    // We just say that all things are removable?

            struct statvfs fstats;

            uint64_t diskSpace = 0;
            uint64_t freeSpace = 0;

            if ( statvfs( mntinfo.mnt_path.GetConstString(), &fstats ) == 0 )
            {
                diskSpace = ( fstats.f_frsize * fstats.f_blocks );
                freeSpace = ( fstats.f_bsize * fstats.f_bavail );
            }

            info.diskSpace = diskSpace;
            info.freeSpace = freeSpace;

            result.AddToBack( std::move( info ) );
        }
#endif

        return result;
    }

    void InitializeActivity( NativeExecutive::CExecutiveManager *execMan ) override
    {
        this->notifyWndThread = execMan->CreateThread( _notify_thread_runtime, this );

        assert( this->notifyWndThread != nullptr );

        this->notifyWndThread->Resume();
    }

    void ShutdownActivity( NativeExecutive::CExecutiveManager *execMan ) noexcept override
    {
        this->notifyWndThread->Terminate( false );

        execMan->JoinThread( this->notifyWndThread );
        execMan->CloseThread( this->notifyWndThread );
    }

    AINLINE void Initialize( CFileSystemNative *fileSys )
    {
        auto *execMan = fileSys->nativeMan;

        if ( execMan == nullptr )
            return;

#ifdef _WIN32
        HINSTANCE curModule = GetModuleHandleA( nullptr );

        WNDCLASSW localAppClass;
        localAppClass.style = 0;
        localAppClass.lpfnWndProc = NotificationWindowProcedure;
        localAppClass.cbClsExtra = 0;
        localAppClass.cbWndExtra = 0;
        localAppClass.hInstance = curModule;
        localAppClass.hIcon = nullptr;
        localAppClass.hCursor = nullptr;
        localAppClass.hbrBackground = nullptr;
        localAppClass.lpszMenuName = L"FileSystem_DiskNotify_Menu";
        localAppClass.lpszClassName = L"FileSystem_DiskNotify";

        this->classNotifyWnd = RegisterClassW( &localAppClass );

        assert( this->classNotifyWnd != 0 );
#endif //_WIN32

        this->lock_notificatorList = execMan->CreateReadWriteLock();

        assert( this->lock_notificatorList != nullptr );

#ifdef _WIN32
        // Get the current mount points at least.
        this->curr_mnt_points = GetCurrentSystemMountPoints();
#elif defined(__linux__)
        // TODO: actually move the mount points init over here so that it can be loaded prior
        // to exiting the creation routine.
#else
#error Missing mount points initialization for platform.
#endif //CROSS PLATFORM CODE

        fileSys->nativeMan->RegisterThreadActivity( this );
    }

    AINLINE void Shutdown( CFileSystemNative *fileSys )
    {
        auto *execMan = fileSys->nativeMan;

        if ( execMan == nullptr )
            return;

        execMan->UnregisterThreadActivity( this );

        execMan->CloseReadWriteLock( this->lock_notificatorList );

#ifdef _WIN32
        UnregisterClassW( (LPCWSTR)MAKEINTATOM( this->classNotifyWnd ), GetModuleHandleA( nullptr ) );
#endif //_WIN32
    }
};

static constinit optional_struct_space <PluginDependantStructRegister <filesysDiskManager, fileSystemFactory_t>> filesysDiskManagerReg;

CFileSystemNative* filesysDiskManager::get_file_system( void )
{
    return filesysDiskManagerReg.get().GetBackResolve( this );
}

const CFileSystemNative* filesysDiskManager::get_file_system( void ) const
{
    return filesysDiskManagerReg.get().GetBackResolve( this );
}

#endif //FILESYS_MULTI_THREADING

fsStaticVector <FileSystem::mountPointInfo> CFileSystem::GetCurrentMountPoints( void ) const
{
    fsStaticVector <FileSystem::mountPointInfo> result;

#ifdef FILESYS_MULTI_THREADING
    CFileSystemNative *nativeMan = (CFileSystemNative*)this;

    const filesysDiskManager *diskMan = filesysDiskManagerReg.get().GetConstPluginStruct( nativeMan );

    result = diskMan->GetCurrentMountPoints();
#endif //FILESYS_MULTI_THREADING

    return result;
}

void CFileSystem::RegisterDiskNotification( CLogicalDiskUpdateNotificationInterface *intf )
{
#ifdef FILESYS_MULTI_THREADING
    CFileSystemNative *nativeMan = (CFileSystemNative*)this;

    filesysDiskManager *diskMan = filesysDiskManagerReg.get().GetPluginStruct( nativeMan );

    NativeExecutive::CReadWriteWriteContext ctx_addnotify( diskMan->lock_notificatorList );

    diskMan->notificatorList.AddToBack( intf );
#else
    throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::RESOURCE_UNAVAILABLE );
#endif //FILESYS_MULTI_THREADING
}

void CFileSystem::UnregisterDiskNotification( CLogicalDiskUpdateNotificationInterface *intf )
{
#ifdef FILESYS_MULTI_THREADING
    CFileSystemNative *nativeMan = (CFileSystemNative*)this;

    filesysDiskManager *diskMan = filesysDiskManagerReg.get().GetPluginStruct( nativeMan );

    NativeExecutive::CReadWriteWriteContext ctx_remnotify( diskMan->lock_notificatorList );

    diskMan->notificatorList.RemoveByValue( intf );
#else
    throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::RESOURCE_UNAVAILABLE );
#endif //FILESYS_MULTI_THREADING
}

bool CFileSystem::GetMountPointDeviceInfo( const wchar_t *query_mntpt, mountPointDeviceInfo& devinfo_out )
{
#ifdef FILESYS_MULTI_THREADING

#ifdef _WIN32
    CFileSystemNative *nativeMan = (CFileSystemNative*)this;

    filesysDiskManager *diskMan = filesysDiskManagerReg.get().GetPluginStruct( nativeMan );

    NativeExecutive::CReadWriteReadContextSafe ctx_readmounts( diskManager_mountPointsLock.get().GetReadWriteLock( nativeMan ) );

    auto mount_points_equal = []( const eir::FixedString <wchar_t>& left, const wchar_t *right )
    {
        return BoundedStringEqual( left, right, false );
    };

    for ( auto& mntptinfo : diskMan->curr_mnt_points )
    {
        for ( auto& mntpath : mntptinfo.mnt_paths )
        {
            if ( mount_points_equal( mntpath.ToFixed(), query_mntpt ) )
            {
                mountPointDeviceInfo devinfo;
                devinfo.volume_guid = mntptinfo.volume_guid;

                devinfo_out = std::move( devinfo );
                return true;
            }
        }
    }
    return false;
#else
    return false;
#endif //PLATFORM DEPENDANT CODE

#else
    return false;
#endif
}

#ifdef _WIN32

struct win32_volume_security_lock
{
    inline win32_volume_security_lock( const win32_deviceGUID& guid )
    {
        // Get the volume GUID path with no trailing backslash.
        auto guiddesc = DeviceUtils::GenerateVolumeGUIDDescriptor <FSObjectHeapAllocator> ( guid );

        DWORD seldom_cnt = 0;

    seldomRetry:
        if ( seldom_cnt >= 3 )
        {
            // We just fail I guess.
            // There could be some driver related issues aswell.
            // But most likely we are here because of open files remaining.
            return;
        }

        // Open an exclusive handle to the device.
        win32Handle hvolume = CreateFileW( guiddesc.GetConstString(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr );

        if ( hvolume.is_good() == false )
        {
#ifdef _DEBUG
            lasterr = GetLastError();
#endif //_DEBUG
            return;
        }

        DWORD bytesret; // unused.

        // Make sure we lock the device down for good.
        BOOL succLock = DeviceIoControl( hvolume, FSCTL_LOCK_VOLUME, nullptr, 0, nullptr, 0, &bytesret, nullptr );

        if ( succLock == FALSE )
        {
            DWORD lasterr = GetLastError();

            if ( lasterr == ERROR_ACCESS_DENIED )
            {
                // Try to dismount the volume.
                BOOL succDismountSub = DeviceIoControl( hvolume, FSCTL_DISMOUNT_VOLUME, nullptr, 0, nullptr, 0, &bytesret, nullptr );

                if ( succDismountSub == FALSE || seldom_cnt >= 1 )
                {
                    // Wait an appropriate amount of time.
                    Sleep( 50 );
                }

                // Go again, from the beginning.
                seldom_cnt++;
                goto seldomRetry;
            }

#ifdef _DEBUG
            this->lasterr = lasterr;
#endif //_DEBUG
            return;
        }

        // Remove file-system presence from any driver.
        // Need to close any open file handles aswell.
        BOOL succDismount = DeviceIoControl( hvolume, FSCTL_DISMOUNT_VOLUME, nullptr, 0, nullptr, 0, &bytesret, nullptr );

        if ( succDismount == FALSE )
        {
#ifdef _DEBUG
            lasterr = GetLastError();
#endif //_DEBUG
            return;
        }

        // Attempt to re-open the volume without sharing.
        win32Handle new_restrictive_handle = ReOpenFile( hvolume, GENERIC_READ | GENERIC_WRITE, 0, 0 );

        if ( new_restrictive_handle )
        {
            hvolume = std::move( new_restrictive_handle );
        }

        BOOL offlinesucc = DeviceIoControl( hvolume, IOCTL_VOLUME_OFFLINE, nullptr, 0, nullptr, 0, &bytesret, nullptr );

        this->was_taken_offline = ( offlinesucc == TRUE );

        this->hvolume = std::move( hvolume );
    }

    inline ~win32_volume_security_lock( void )
    {
        DWORD bytesret;

        if ( this->was_taken_offline )
        {
            DeviceIoControl( hvolume, IOCTL_VOLUME_ONLINE, nullptr, 0, nullptr, 0, &bytesret, nullptr );
        }

        DeviceIoControl( hvolume, FSCTL_UNLOCK_VOLUME, nullptr, 0, nullptr, 0, &bytesret, nullptr );
    }

    inline bool is_good( void )
    {
        return ( this->hvolume );
    }

private:
    win32Handle hvolume;
    bool was_taken_offline;

#ifdef _DEBUG
    DWORD lasterr;
#endif //_DEBUG
};

#endif //_WIN32

bool CFileSystem::MountDevice( const mountPointDeviceInfo& devinfo, const wchar_t *mntpt )
{
#ifdef _WIN32
    win32_volume_security_lock seclock( devinfo.volume_guid );

    if ( seclock.is_good() == false )
        return false;

    auto volguidpath = DeviceUtils::GenerateVolumeGUIDPath <FSObjectHeapAllocator> ( devinfo.volume_guid );

    BOOL mntres = SetVolumeMountPointW( mntpt, volguidpath.GetConstString() );

    return ( mntres == TRUE );
#else
    return false;
#endif //PLATFORM DEPENDENT CODE
}

bool CFileSystem::RemountDevice( const mountPointDeviceInfo& devinfo, const wchar_t *oldmnt, const wchar_t *newmnt )
{
#ifdef _WIN32
    win32_volume_security_lock seclock( devinfo.volume_guid );

    if ( seclock.is_good() == false )
        return false;

    BOOL unmntres = DeleteVolumeMountPointW( oldmnt );

    if ( unmntres == FALSE )
        return false;

    auto volguidpath = DeviceUtils::GenerateVolumeGUIDPath <FSObjectHeapAllocator> ( devinfo.volume_guid );

    BOOL mntres = SetVolumeMountPointW( newmnt, volguidpath.GetConstString() );

    return ( mntres == TRUE );
#else
    return false;
#endif //PLATFORM DEPENDENT CODE
}

bool CFileSystem::UnmountDevice( const mountPointDeviceInfo& devInfo, const wchar_t *mntpt )
{
#ifdef _WIN32
    win32_volume_security_lock seclock( devInfo.volume_guid );

    if ( seclock.is_good() == false )
        return false;

    BOOL unmntres = DeleteVolumeMountPointW( mntpt );

    return ( unmntres == TRUE );
#else
    return false;
#endif //PLATFORM DEPENDENT CODE
}

void registerDiskManagement( const fs_construction_params& params )
{
#ifdef FILESYS_MULTI_THREADING
    diskManager_mountPointsLock.Construct( params );
    filesysDiskManagerReg.Construct( _fileSysFactory );
#endif //FILESYS_MULTI_THREADING
}

void unregisterDiskManagement( void )
{
#ifdef FILESYS_MULTI_THREADING
    filesysDiskManagerReg.Destroy();
    diskManager_mountPointsLock.Destroy();
#endif //FILESYS_MULTI_THREADING
}
