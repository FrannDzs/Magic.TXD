#ifndef _SHELL_IDLTOOLS_
#define _SHELL_IDLTOOLS_

#include <string>
#include <limits>
#include <stdint.h>

#ifndef AINLINE
#ifdef _WIN32
#define AINLINE __forceinline
#else
#define AINLINE inline
#endif //_WIN32
#endif //AINLINE

namespace idltools
{

struct serializerException
{};

template <typename serType>
struct _serializedDataMan
{
};

#define MAKE_POD_SIZEDEF(type) \
template <> \
struct _serializedDataMan <type> \
{ \
    AINLINE static size_t size( const type& data ) \
    { return sizeof( data ); } \
    AINLINE static void moveTo( void *dst, type& data, size_t givenSize ) \
    { *(type*)dst = std::move( data ); } \
};

MAKE_POD_SIZEDEF( char );
MAKE_POD_SIZEDEF( short );
MAKE_POD_SIZEDEF( int );
MAKE_POD_SIZEDEF( long long );
MAKE_POD_SIZEDEF( unsigned char );
MAKE_POD_SIZEDEF( unsigned short );
MAKE_POD_SIZEDEF( unsigned int );
MAKE_POD_SIZEDEF( unsigned long long );
MAKE_POD_SIZEDEF( float );
MAKE_POD_SIZEDEF( double );

// For special containers like strings.
template <typename stringContType>
struct _serializedDataManSTLString
{
    struct header
    {
        std::uint16_t charCount;
    };

    AINLINE static size_t size( const stringContType& data )
    {
        if ( data.size() > std::numeric_limits <decltype( header::charCount )>::max() )
        {
            throw serializerException();
        }

        size_t actualSize = 0;

        // Make a header with the character count.
        actualSize += sizeof( header );

        // Add the characters next.
        actualSize += data.size() * sizeof( decltype( *data.c_str() ) );

        return actualSize;
    }

    AINLINE static void moveTo( void *ptr, const stringContType& data, size_t givenSize )
    {
        header *dataHeader = (header*)ptr;

        dataHeader->charCount = data.size();

        void *charData = ( dataHeader + 1 );

        memcpy( charData, data.c_str(), givenSize - sizeof( header ) );
    }
};

template <>
struct _serializedDataMan <std::string> : public _serializedDataManSTLString <std::string>
{};
template <>
struct _serializedDataMan <std::wstring> : public _serializedDataManSTLString <std::wstring>
{};

template <typename charType>
struct _serializedDataManCString
{
    struct header
    {
        std::uint16_t charCount;
    };

    AINLINE static size_t size( const charType* const& data )
    {
        size_t len = std::char_traits <charType>().length( data );

        if ( len > std::numeric_limits <decltype( header::charCount )>::max() )
        {
            throw serializerException();
        }

        size_t actualSize = 0;

        // Make a header with the character count.
        actualSize += sizeof( header );

        // Add the characters next.
        actualSize += len * sizeof( charType );

        return actualSize;
    }

    AINLINE static void moveTo( void *ptr, const charType* const& data, size_t givenSize )
    {
        header *dataHeader = (header*)ptr;

        std::uint16_t charCount = (std::uint16_t)( ( givenSize - sizeof( header ) ) / sizeof( charType ) );

        dataHeader->charCount = charCount;

        void *charData = ( dataHeader + 1 );

        memcpy( charData, data, givenSize - sizeof( header ) );
    }
};

template <>
struct _serializedDataMan <const char*> : public _serializedDataManCString <char>
{};
template <>
struct _serializedDataMan <const wchar_t*> : public _serializedDataManCString <wchar_t>
{};

// Allocate memory based on types given.
template <typename singleType, typename accumTypeCont, typename... idlTypes>
AINLINE size_t _GraduateSizeAccum( accumTypeCont& metaCont, size_t startOff, singleType type, idlTypes... otherTypes )
{
    metaCont.offset = startOff;
    metaCont.size = _serializedDataMan <singleType>::size( type );

    const size_t nextOff = ( metaCont.offset + metaCont.size );

    return _GraduateSizeAccum( metaCont.next, nextOff, otherTypes... );
}

template <typename accumTypeCont>
AINLINE size_t _GraduateSizeAccum( accumTypeCont& metaCont, size_t startOff )
{
    // We should align to DWORD.
    return ( startOff & ~( sizeof( std::uint32_t ) - 1 ) );
}

template <typename... someTypes>
struct sizeContainerAccum
{};

template <typename curType, typename... remainingTypes>
struct sizeContainerAccum <curType, remainingTypes...>
{
    size_t offset;
    size_t size;

    sizeContainerAccum <remainingTypes...> next;
};

template <typename metaDataType, typename currentDataType, typename... nextDataTypes>
AINLINE void _MoveDataOntoPlace( const metaDataType& metaData, void *ptr, currentDataType& data, nextDataTypes... next )
{
    size_t curOffset = metaData.offset;
    size_t curSize = metaData.size;

    // Place it.
    void *curPtr = ( (char*)ptr + curOffset );

    _serializedDataMan <currentDataType>::moveTo( curPtr, data, curSize );

    // Process more stuff.
    _MoveDataOntoPlace( metaData.next, ptr, next... );
}

template <typename metaDataType>
AINLINE void _MoveDataOntoPlace( const metaDataType& metaData, void *ptr )
{
    return;
}

// We need a way to "manage" IDLs.
// This means allocating them and actually parsing potential IDLs.
template <typename headerType, typename... idlTypes>
AINLINE headerType* AllocateGraduateContainer( idlTypes... args )
{
    sizeContainerAccum <idlTypes...> dataMeta;

    size_t wholeSize = _GraduateSizeAccum( dataMeta, sizeof( headerType ), args... );

    // We must not overshoot numeric limits.
    if ( wholeSize > std::numeric_limits <typename headerType::sizeType_t>::max() )
    {
        return NULL;
    }

    typename headerType::sizeType_t valid_size = (typename headerType::sizeType_t)wholeSize;

    void *objMem = headerType::allocate( valid_size );

    if ( !objMem )
    {
        return NULL;
    }

    headerType *dataObj = new (objMem) headerType( valid_size );

    // Place things onto it.
    _MoveDataOntoPlace( dataMeta, dataObj, args... );

    return dataObj;
}

}

#endif //_SHELL_IDLTOOLS_
