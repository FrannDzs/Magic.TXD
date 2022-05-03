/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.blockapi.h
*  PURPOSE:     RenderWare block serialization helpers.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

namespace rw
{

enum eBlockMode
{
    RWBLOCKMODE_WRITE,
    RWBLOCKMODE_READ
};

enum class eBlockAcquisitionMode : uint32
{
    EXPECTED,       // the next block in reading order must match chunk ID
    FIND            // any block in serialization chain must match chunk ID
};

enum class blockprov_constr_no_ctx
{
    DEFAULT
};

struct BlockProvider
{
    typedef sliceOfData <int64> streamMemSlice_t;

private:
    AINLINE void initialize_with_parent( BlockProvider *parentProvider )
    {
#ifdef _DEBUG
        assert( parentProvider != nullptr );
        assert( parentProvider->current_child == nullptr );
#endif //_DEBUG

        parentProvider->current_child = this;

        this->parent = parentProvider;
        this->current_child = nullptr;
        this->blockMode = parentProvider->blockMode;
        this->isInContext = false;
        this->contextStream = nullptr;
        this->isInErrorCondition = parentProvider->isInErrorCondition;
    }

public:
    // Main constructors to use for the root block provider.
    BlockProvider( Stream *contextStream, rw::eBlockMode blockMode );
    BlockProvider( Stream *contextStream, rw::eBlockMode blockMode, bool ignoreBlockRegions, eBlockAcquisitionMode acqMode );

    inline BlockProvider( void ) noexcept
    {
        this->parent = nullptr;
        this->current_child = nullptr;
        this->blockMode = RWBLOCKMODE_READ;
        this->isInContext = false;
        this->contextStream = nullptr;
        this->isInErrorCondition = false;
        this->ignoreBlockRegions = false;
        this->acquisitionMode = eBlockAcquisitionMode::EXPECTED;
        this->hasConstructorContext = false;
    }

    inline BlockProvider( BlockProvider *parentProvider )
    {
        this->initialize_with_parent( parentProvider );
        this->ignoreBlockRegions = parentProvider->ignoreBlockRegions;
        this->acquisitionMode = parentProvider->acquisitionMode;
        this->hasConstructorContext = true;

        this->EnterContextAny();
    }

    inline BlockProvider( blockprov_constr_no_ctx _, BlockProvider *parentProvider )
    {
        this->initialize_with_parent( parentProvider );
        this->ignoreBlockRegions = parentProvider->ignoreBlockRegions;
        this->acquisitionMode = parentProvider->acquisitionMode;
        this->hasConstructorContext = false;
    }

    inline BlockProvider( BlockProvider *parentProvider, uint32 chunk_id )
    {
        this->initialize_with_parent( parentProvider );
        this->ignoreBlockRegions = parentProvider->ignoreBlockRegions;
        this->acquisitionMode = parentProvider->acquisitionMode;
        this->hasConstructorContext = true;

        this->EnterContextDirect( chunk_id );
    }

    inline BlockProvider( BlockProvider *parentProvider, uint32 chunk_id, bool ignoreBlockRegions )
    {
        this->initialize_with_parent( parentProvider );
        this->ignoreBlockRegions = ignoreBlockRegions;
        this->acquisitionMode = parentProvider->acquisitionMode;
        this->hasConstructorContext = true;

        this->EnterContextDirect( chunk_id );
    }

    inline BlockProvider( blockprov_constr_no_ctx _, BlockProvider *parentProvider, bool ignoreBlockRegions )
    {
        this->initialize_with_parent( parentProvider );
        this->ignoreBlockRegions = ignoreBlockRegions;
        this->acquisitionMode = parentProvider->acquisitionMode;
        this->hasConstructorContext = false;
    }

    inline BlockProvider( BlockProvider *parentProvider, uint32 chunk_id, bool ignoreBlockRegions, eBlockAcquisitionMode acqMode )
    {
        this->initialize_with_parent( parentProvider );
        this->ignoreBlockRegions = ignoreBlockRegions;
        this->acquisitionMode = acqMode;
        this->hasConstructorContext = true;

        this->EnterContextDirect( chunk_id );
    }

private:
    void moveFrom( BlockProvider&& right ) noexcept;

public:
    BlockProvider( const BlockProvider& ) = delete;
    BlockProvider( BlockProvider&& right ) noexcept;

private:
    void signal_blockapi_error( void ) noexcept;
    void check_error_condition( void ) const;
    
    // This function could throw an exception but in very rare circumstances only.
    AINLINE void clear( bool isExceptionFatal )
    {
        bool hadConstructorContext = this->hasConstructorContext;

        this->hasConstructorContext = false;

        if ( hadConstructorContext )
        {
            this->LeaveContext( isExceptionFatal == false );
        }

        assert( this->isInContext == false );
    }

public:
    // Could be fun to allow for two destructors: one with noexcept(false) and one with noexcept(true).
    // Then we could use the noexcept(true) one inside of eir::Vector for safe destruction, but the
    // noexcept(false) one during regular stack usage.
    inline ~BlockProvider( void )
    {
        clear( true );

        if ( BlockProvider *parentProvider = this->parent )
        {
            parentProvider->current_child = nullptr;
        }
    }

    BlockProvider& operator = ( const BlockProvider& ) = delete;
    BlockProvider& operator = ( BlockProvider&& right ) noexcept;

protected:
    BlockProvider *parent;
    BlockProvider *current_child;   // there can be only one.

    eBlockMode blockMode;
    bool isInContext;
    bool hasConstructorContext;

    Stream *contextStream;

    bool ignoreBlockRegions;
    eBlockAcquisitionMode acquisitionMode;

    bool isInErrorCondition;

    // Processing context of this stream.
    // This is stored for important points.
    struct Context
    {
        inline Context( void )
        {
            this->chunk_id = 0;
            this->chunk_beg_offset = -1;
            this->chunk_beg_offset_absolute = -1;
            this->chunk_length = -1;

            this->context_seek = -1;
        }

        uint32 chunk_id;
        int64 chunk_beg_offset;
        int64 chunk_beg_offset_absolute;
        int64 chunk_length;

        int64 context_seek;
        
        LibraryVersion chunk_version;
    };

    Context blockContext;

    void _EnterContextWrite( uint32 chunk_id );
    void _InternalContextStart( void );

public:
    // Entering a block (i.e. parsing the header and setting limits).
    void EnterContextAny( void );
    void EnterContextDirect( uint32 chunk_id );
    void LeaveContext( bool canPropagateException = true );

    AINLINE void EstablishObjectContextAny( void )
    {
#ifdef _DEBUG
        assert( this->hasConstructorContext == false );
#endif //_DEBUG

        this->EnterContextAny();

        this->hasConstructorContext = true;
    }

    AINLINE void EstablishObjectContextDirect( uint32 chunk_id )
    {
#ifdef _DEBUG
        assert( this->hasConstructorContext == false );
#endif //_DEBUG

        this->EnterContextDirect( chunk_id );

        this->hasConstructorContext = true;
    }

    inline bool inContext( void ) const
    {
        return this->isInContext;
    }

    // Block modification API.
    void read( void *out_buf, size_t readCount );
    size_t read_partial( void *out_buf, size_t readCount );
    void write( const void *in_buf, size_t writeCount );

    void skip( size_t skipCount );
    int64 tell( void ) const;
    int64 tell_absolute( void ) const;

    void seek( int64 pos, eSeekMode mode );
    void seek_absolute( int64 pos );

    // Public verification API.
    void check_read_ahead( size_t readCount ) const;

protected:
    // Special helper algorithms.
    void read_native( void *out_buf, size_t readCount );
    size_t read_partial_native( void *out_buf, size_t readCount );
    void write_native( const void *in_buf, size_t writeCount );

    void skip_native( size_t skipCount );
    
    void set_seek_internal( int64 pos );
    void seek_native( int64 pos, eSeekMode mode );
    int64 tell_native( void ) const;
    int64 tell_absolute_native( void ) const;

    Interface* getEngineInterface( void ) const;

public:
    // Block meta-data API.
    uint32 getBlockID( void ) const;
    int64 getBlockLength( void ) const;
    LibraryVersion getBlockVersion( void ) const;

    void setBlockID( uint32 id );
    void setBlockVersion( LibraryVersion version );

    bool doesIgnoreBlockRegions( void ) const;
    eBlockAcquisitionMode getBlockAcquisitionMode( void ) const;

    bool hasParent( void ) const;

    // Helper functions.
    template <typename structType>
    inline void writeStruct( const structType& theStruct )      { this->write( &theStruct, sizeof( theStruct ) ); }

    inline void writeUInt8( endian::little_endian <uint8> val )     { this->writeStruct( val ); }
    inline void writeUInt16( endian::little_endian <uint16> val )   { this->writeStruct( val ); }
    inline void writeUInt32( endian::little_endian <uint32> val )   { this->writeStruct( val ); }
    inline void writeUInt64( endian::little_endian <uint64> val )   { this->writeStruct( val ); }

    inline void writeInt8( endian::little_endian <int8> val )       { this->writeStruct( val );}
    inline void writeInt16( endian::little_endian <int16> val )     { this->writeStruct( val ); }
    inline void writeInt32( endian::little_endian <int32> val )     { this->writeStruct( val ); }
    inline void writeInt64( endian::little_endian <int64> val )     { this->writeStruct( val ); }

    template <typename structType>
    inline void readStruct( structType& outStruct )                 { this->read( &outStruct, sizeof( outStruct ) ); }

    inline endian::little_endian <uint8> readUInt8( void )          { endian::little_endian <uint8> val; this->readStruct( val ); return val; }
    inline endian::little_endian <uint16> readUInt16( void )        { endian::little_endian <uint16> val; this->readStruct( val ); return val; }
    inline endian::little_endian <uint32> readUInt32( void )        { endian::little_endian <uint32> val; this->readStruct( val ); return val; }
    inline endian::little_endian <uint64> readUInt64( void )        { endian::little_endian <uint64> val; this->readStruct( val ); return val; }

    inline endian::little_endian <int8> readInt8( void )            { endian::little_endian <int8> val; this->readStruct( val ); return val; }
    inline endian::little_endian <int16> readInt16( void )          { endian::little_endian <int16> val; this->readStruct( val ); return val; }
    inline endian::little_endian <int32> readInt32( void )          { endian::little_endian <int32> val; this->readStruct( val ); return val; }
    inline endian::little_endian <int64> readInt64( void )          { endian::little_endian <int64> val; this->readStruct( val ); return val; }

protected:
    // Validation API.
    void verifyLocalStreamAccess( streamMemSlice_t& requestedMemory, bool allowPartial = false ) const;
    void verifyStreamAccess( streamMemSlice_t& requestedMemory, bool allowPartial = false ) const;
};

} // namespace rw