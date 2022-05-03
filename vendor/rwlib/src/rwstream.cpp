/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwstream.cpp
*  PURPOSE:     Stream object generic implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "pluginutil.hxx"

#include <sdk/MemoryUtils.stream.h>

namespace rw
{

size_t Stream::read( void *out_buf, size_t readCount )
{
    throw UnsupportedOperationException( eSubsystemType::STREAM, eOperationType::READ, L"REASON_NOTIMPLEMENTED" );
}

size_t Stream::write( const void *in_buf, size_t writeCount )
{
    throw UnsupportedOperationException( eSubsystemType::STREAM, eOperationType::WRITE, L"REASON_NOTIMPLEMENTED" );
}

void Stream::skip( int64 skipCount )
{
    throw UnsupportedOperationException( eSubsystemType::STREAM, eOperationType::SKIP, nullptr );
}

int64 Stream::tell( void ) const
{
    return -1;
}

void Stream::seek( int64 seek_off, eSeekMode seek_mode )
{
    throw UnsupportedOperationException( eSubsystemType::STREAM, eOperationType::SEEK, nullptr );
}

int64 Stream::size( void ) const
{
    throw UnsupportedOperationException( eSubsystemType::STREAM, eOperationType::GETSIZE, nullptr );
}

bool Stream::supportsSize( void ) const
{
    return false;
}

// File stream.
struct FileStream final : public Stream
{
    inline FileStream( Interface *engineInterface, void *construction_params ) : Stream( engineInterface, construction_params )
    {
        this->file_handle = nullptr;
    }

    inline ~FileStream( void )
    {
        // Clear the file (if allocated).
        if ( Interface *engineInterface = this->engineInterface )
        {
            if ( FileInterface::filePtr_t fileHandle = this->file_handle )
            {
                engineInterface->GetFileInterface()->CloseStream( fileHandle );
            }
        }
    }

    // Stream methods.
    size_t read( void *out_buf, size_t readCount ) override
    {
        size_t actualReadCount = 0;

        if ( Interface *engineInterface = this->engineInterface )
        {
            actualReadCount = engineInterface->GetFileInterface()->ReadStream( this->file_handle, out_buf, readCount );
        }

        return actualReadCount;
    }

    size_t write( const void *in_buf, size_t writeCount ) override
    {
        size_t actualWriteCount = 0;

        if ( Interface *engineInterface = this->engineInterface )
        {
            actualWriteCount = engineInterface->GetFileInterface()->WriteStream( this->file_handle, in_buf, writeCount );
        }

        return actualWriteCount;
    }

    void skip( int64 skipCount ) override
    {
        if ( Interface *engineInterface = this->engineInterface )
        {
            engineInterface->GetFileInterface()->SeekStream( this->file_handle, (long)skipCount, SEEK_CUR );
        }
    }

    // Advanced functions.
    int64 tell( void ) const override
    {
        int64 curSeek = 0;

        if ( Interface *engineInterface = this->engineInterface )
        {
            curSeek = engineInterface->GetFileInterface()->TellStream( this->file_handle );
        }

        return curSeek;
    }

    void seek( int64 seek_off, eSeekMode seek_mode ) override
    {
        if ( Interface *engineInterface = this->engineInterface )
        {
            int ansi_seek = SEEK_CUR;

            if ( seek_mode == RWSEEK_BEG )
            {
                ansi_seek = SEEK_SET;
            }
            else if ( seek_mode == RWSEEK_CUR )
            {
                ansi_seek = SEEK_CUR;
            }
            else if ( seek_mode == RWSEEK_END )
            {
                ansi_seek = SEEK_END;
            }   

            engineInterface->GetFileInterface()->SeekStream( this->file_handle, (long)seek_off, ansi_seek );
        }
    }

    int64 size( void ) const override
    {
        int64 sizeOut = 0;

        if ( Interface *engineInterface = this->engineInterface )
        {
            sizeOut = engineInterface->GetFileInterface()->SizeStream( this->file_handle );
        }

        return sizeOut;
    }

    bool supportsSize( void ) const override
    {
        return true;
    }

    FileInterface::filePtr_t file_handle;
};

// Memory stream.
struct FixedBufferMemoryStream final : public Stream
{
    struct constrParams
    {
        void *buf;
        size_t bufSize;
        eStreamMode streamMode;
    };

    inline FixedBufferMemoryStream( Interface *engineInterface, void *construction_params ) :
        Stream( engineInterface, construction_params ),
        memStream( nullptr, 0, streamMan )
    {
        constrParams *params = (constrParams*)construction_params;
        this->memStream = decltype(memStream)( params->buf, params->bufSize, streamMan );
        this->streamMode = params->streamMode;
    }

    size_t read( void *out_buf, size_t readCount ) override
    {
        eStreamMode streamMode = this->streamMode;

        if ( streamMode == RWSTREAMMODE_READONLY || streamMode == RWSTREAMMODE_READWRITE )
        {
            return this->memStream.Read( out_buf, readCount );
        }

        return 0;
    }

    size_t write( const void *in_buf, size_t writeCount ) override
    {
        eStreamMode streamMode = this->streamMode;

        if ( streamMode == RWSTREAMMODE_READWRITE || streamMode == RWSTREAMMODE_WRITEONLY || streamMode == RWSTREAMMODE_CREATE )
        {
            return this->memStream.Write( in_buf, writeCount );
        }

        return 0;
    }

    void skip( int64 skipCount ) override
    {
        int64 curSeek = this->memStream.Tell();

        this->memStream.Seek( curSeek + skipCount );
    }

    int64 tell( void ) const override
    {
        return this->memStream.Tell();
    }

    void seek( int64 seek_off, eSeekMode seek_mode ) override
    {
        int64 base_off = 0;

        if ( seek_mode == rw::RWSEEK_BEG )
        {
            base_off = 0;
        }
        else if ( seek_mode == rw::RWSEEK_CUR )
        {
            base_off = this->memStream.Tell();
        }
        else if ( seek_mode == rw::RWSEEK_END )
        {
            base_off = this->memStream.Size();
        }
        else
        {
            throw InvalidParameterException( eSubsystemType::STREAM, L"STREAM_INVALIDPARAM_SEEKMODE", nullptr );
        }

        this->memStream.Seek( base_off + seek_off );
    }

    int64 size( void ) const override
    {
        return this->memStream.Size();
    }

    bool supportsSize( void ) const override
    {
        return true;
    }

    struct memStreamManager
    {
        AINLINE static void EstablishBufferView( void*& memPtrInOut, int64& sizeInOut, int64 reqNewSize )
        {
            // We do nothing because this stream type is supposed to be immutable in size.
            return;
        }
    };

    memStreamManager streamMan;
    memoryBufferStream <int64, memStreamManager, false, false> memStream;
    eStreamMode streamMode;
};

struct DynamicBufferMemoryStream final : public Stream
{
    struct constrParams
    {
        eStreamMode streamMode;
    };

    inline DynamicBufferMemoryStream( Interface *engineInterface, void *construction_params ) :
        Stream( engineInterface, construction_params ),
        memStream( nullptr, 0, streamMan )
    {
        constrParams *params = (constrParams*)construction_params;
        this->streamMode = params->streamMode;
    }

    size_t read( void *out_buf, size_t readCount ) override
    {
        eStreamMode streamMode = this->streamMode;

        if ( streamMode == RWSTREAMMODE_READONLY || streamMode == RWSTREAMMODE_READWRITE )
        {
            return this->memStream.Read( out_buf, readCount );
        }

        return 0;
    }

    size_t write( const void *in_buf, size_t writeCount ) override
    {
        eStreamMode streamMode = this->streamMode;

        if ( streamMode == RWSTREAMMODE_READWRITE || streamMode == RWSTREAMMODE_WRITEONLY || streamMode == RWSTREAMMODE_CREATE )
        {
            return this->memStream.Write( in_buf, writeCount );
        }

        return 0;
    }

    void skip( int64 skipCount ) override
    {
        int64 curSeek = this->memStream.Tell();

        this->memStream.Seek( curSeek + skipCount );
    }

    int64 tell( void ) const override
    {
        return this->memStream.Tell();
    }

    void seek( int64 seek_off, eSeekMode seek_mode ) override
    {
        int64 base_off = 0;

        if ( seek_mode == rw::RWSEEK_BEG )
        {
            base_off = 0;
        }
        else if ( seek_mode == rw::RWSEEK_CUR )
        {
            base_off = this->memStream.Tell();
        }
        else if ( seek_mode == rw::RWSEEK_END )
        {
            base_off = this->memStream.Size();
        }
        else
        {
            throw InvalidParameterException( eSubsystemType::STREAM, L"STREAM_INVALIDPARAM_SEEKMODE", nullptr );
        }

        this->memStream.Seek( base_off + seek_off );
    }

    int64 size( void ) const override
    {
        return this->memStream.Size();
    }

    bool supportsSize( void ) const override
    {
        return true;
    }

    struct memStreamManager
    {
        AINLINE void EstablishBufferView( void*& memPtrInOut, int64& sizeInOut, int64 reqNewSize )
        {
            Interface *engineInterface = this->GetEngineInterface();

            void *memPtr = memPtrInOut;

            if ( reqNewSize == 0 )
            {
                if ( memPtr != nullptr )
                {
                    engineInterface->MemFree( memPtr );

                    memPtrInOut = nullptr;
                    sizeInOut = 0;
                }
            }
            else if ( reqNewSize > 0 )
            {
                uint64 unsignedReqNewSize = (uint64)reqNewSize;

                if ( unsignedReqNewSize <= std::numeric_limits <size_t>::max() )
                {
                    size_t validReqNewSize = (size_t)unsignedReqNewSize;

                    if ( memPtr )
                    {
                        bool couldResize = engineInterface->MemResize( memPtr, validReqNewSize );

                        if ( couldResize )
                        {
                            sizeInOut = reqNewSize;
                            return;
                        }
                    }

                    void *newPtr = engineInterface->MemAllocateP( validReqNewSize, 1 );

                    if ( newPtr )
                    {
                        if ( memPtr != nullptr )
                        {
                            size_t oldSize = (size_t)sizeInOut;

                            FSDataUtil::copy_impl <char> ( (const char*)memPtr, (const char*)memPtr + oldSize, (char*)newPtr );
                        }

                        memPtrInOut = newPtr;
                        sizeInOut = reqNewSize;
                    }
                }
            }
        }

        AINLINE Interface* GetEngineInterface( void );
    };

    memStreamManager streamMan;
    memoryBufferStream <int64, memStreamManager, false> memStream;
    eStreamMode streamMode;
};

Interface* DynamicBufferMemoryStream::memStreamManager::GetEngineInterface( void )
{
    DynamicBufferMemoryStream *stream = LIST_GETITEM( DynamicBufferMemoryStream, this, streamMan );

    return stream->engineInterface;
}

// Custom stream.
// This is a simple wrapper so that every implementation can create native RenderWare streams without knowing the internals.
struct CustomStream : public Stream
{
    inline CustomStream( Interface *engineInterface, void *construction_params ) : Stream( engineInterface, construction_params )
    {
        this->streamProvider = nullptr;
    }

    size_t read( void *out_buf, size_t readCount )
    {
        size_t actualReadCount = 0;

        if ( customStreamInterface *streamProvider = this->streamProvider )
        {
            void *metaBuf = ( this + 1 );

            actualReadCount = streamProvider->Read( metaBuf, out_buf, readCount );
        }

        return actualReadCount;
    }

    size_t write( const void *in_buf, size_t writeCount )
    {
        size_t actualWriteCount = 0;

        if ( customStreamInterface *streamProvider = this->streamProvider )
        {
            void *metaBuf = ( this + 1 );

            actualWriteCount = streamProvider->Write( metaBuf, in_buf, writeCount );
        }

        return actualWriteCount;
    }

    void skip( int64 skipCount )
    {
        if ( customStreamInterface *streamProvider = this->streamProvider )
        {
            void *metaBuf = ( this + 1 );

            streamProvider->Skip( metaBuf, skipCount );
        }
    }

    int64 tell( void ) const
    {
        int64 streamPos = -1;

        if ( customStreamInterface *streamProvider = this->streamProvider )
        {
            const void *metaBuf = ( this + 1 );

            streamPos = streamProvider->Tell( metaBuf );
        }

        return streamPos;
    }

    void seek( int64 seek_off, eSeekMode seek_mode )
    {
        if ( customStreamInterface *streamProvider = this->streamProvider )
        {
            void *metaBuf = ( this + 1 );

            streamProvider->Seek( metaBuf, seek_off, seek_mode );
        }
    }

    int64 size( void ) const
    {
        int64 streamSize = -1;

        if ( customStreamInterface *streamProvider = this->streamProvider )
        {
            const void *metaBuf = ( this + 1 );

            streamSize = streamProvider->Size( metaBuf );
        }

        return streamSize;
    }

    bool supportsSize( void ) const
    {
        bool supportsSize = false;

        if ( customStreamInterface *streamProvider = this->streamProvider )
        {
            const void *metaBuf = ( this + 1 );

            supportsSize = streamProvider->SupportsSize( metaBuf );
        }

        return supportsSize;
    }

    customStreamInterface *streamProvider;
};

// Register the stream plugin to the interface.
struct streamSystemPlugin
{
    inline streamSystemPlugin( EngineInterface *engine ) : custom_types( eir::constr_with_alloc::DEFAULT, engine )
    {
        return;
    }

    inline void Initialize( EngineInterface *engine )
    {
        this->fileStreamTypeInfo = nullptr;
        this->memoryStreamTypeInfo = nullptr;

        if ( engine->streamTypeInfo != nullptr )
        {
            this->fileStreamTypeInfo = engine->typeSystem.RegisterStructType <FileStream> ( "file_stream", engine->streamTypeInfo );
            this->memoryStreamTypeInfo = engine->typeSystem.RegisterStructType <FixedBufferMemoryStream> ( "memory_stream", engine->streamTypeInfo );
            this->memexpandStreamTypeInfo = engine->typeSystem.RegisterStructType <DynamicBufferMemoryStream> ( "dyn_memory_stream", engine->streamTypeInfo );
        }

        this->streamEnvLock = rw::CreateReadWriteLock( engine );
    }

    inline void Shutdown( EngineInterface *engine )
    {
        if ( rwlock *lock = this->streamEnvLock )
        {
            rw::CloseReadWriteLock( engine, lock );
        }

        // Delete all custom types first.
        size_t numTypes = this->custom_types.GetCount();

        for ( size_t n = 0; n < numTypes; n++ )
        {
            RwTypeSystem::typeInfoBase *theType = this->custom_types[ n ];

            engine->typeSystem.DeleteType( theType );
        }
        custom_types.Clear();

        if ( RwTypeSystem::typeInfoBase *fileStreamTypeInfo = this->fileStreamTypeInfo )
        {
            engine->typeSystem.DeleteType( fileStreamTypeInfo );
        }

        if ( RwTypeSystem::typeInfoBase *memoryStreamTypeInfo = this->memoryStreamTypeInfo )
        {
            engine->typeSystem.DeleteType( memoryStreamTypeInfo );
        }

        if ( RwTypeSystem::typeInfoBase *memexpandStreamTypeInfo = this->memexpandStreamTypeInfo )
        {
            engine->typeSystem.DeleteType( memexpandStreamTypeInfo );
        }
    }

    // Built-in stream types.
    RwTypeSystem::typeInfoBase *fileStreamTypeInfo;
    RwTypeSystem::typeInfoBase *memoryStreamTypeInfo;
    RwTypeSystem::typeInfoBase *memexpandStreamTypeInfo;
    
    // Custom stream types.
    rwVector <RwTypeSystem::typeInfoBase*> custom_types;

    // Thread-safety lock.
    rw::rwlock *streamEnvLock;
};

static optional_struct_space <PluginDependantStructRegister <streamSystemPlugin, RwInterfaceFactory_t>> streamSystemPluginRegister;

struct customStreamConstructionParams
{
    eStreamMode streamMode;
    void *userdata;
};

struct streamCustomTypeInterface : public RwTypeSystem::typeInterface
{
    AINLINE streamCustomTypeInterface( size_t objCustomBufferSize, size_t objCustomBufferAlignment, customStreamInterface *meta_info )
    {
        this->objCustomBufferSize = objCustomBufferSize;
        this->objCustomBufferAlignment = objCustomBufferAlignment;
        this->meta_info = meta_info;
    }

    void Construct( void *mem, EngineInterface *engineInterface, void *construction_params ) const override
    {
        // We must receive a custom userdata struct.
        customStreamConstructionParams *custom_param = (customStreamConstructionParams*)construction_params;

        // First construct the actual object.
        new (mem) CustomStream( engineInterface, nullptr );

        try
        {
            // Next construct the custom interface.
            void *customBuf = GetObjectImplementationPointerForObject( mem, sizeof(CustomStream), this->objCustomBufferAlignment );

            this->meta_info->OnConstruct( custom_param->streamMode, custom_param->userdata, customBuf, this->objCustomBufferSize );
        }
        catch( ... )
        {
            // Destroy the main class again, since an exception was thrown.
            ( (CustomStream*)mem )->~CustomStream();

            throw;
        }
    }

    void CopyConstruct( void *mem, const void *srcMem ) const override
    {
        // Impossible.
        throw InvalidOperationException( eSubsystemType::STREAM, eOperationType::COPY_CONSTRUCT, L"STREAM_CUSTOM_FRIENDLYNAME" );
    }

    void Destruct( void *mem ) const override
    {
        // First delete the meta struct.
        {
            void *customBuf = (CustomStream*)mem + 1;

            this->meta_info->OnDestruct( customBuf, this->objCustomBufferSize );
        }

        // Now delete the actual object.
        ( (CustomStream*)mem )->~CustomStream();
    }

    size_t GetTypeSize( EngineInterface *engineInterface, void *construct_params ) const override final
    {
        return CalculateAlignedTypeSizeWithHeader( sizeof(CustomStream), this->objCustomBufferSize, this->objCustomBufferAlignment );
    }

    size_t GetTypeSizeByObject( EngineInterface *engineInterface, const void *mem ) const override final
    {
        return this->GetTypeSize( engineInterface, nullptr );
    }

    size_t GetTypeAlignment( EngineInterface *engineInterface, void *construct_params ) const override final
    {
        return CalculateAlignmentForObjectWithHeader( alignof(CustomStream), this->objCustomBufferAlignment );
    }

    size_t GetTypeAlignmentByObject( EngineInterface *engineInterface, const void *mem ) const override final
    {
        return this->GetTypeAlignment( engineInterface, nullptr );
    }

    size_t objCustomBufferSize;
    size_t objCustomBufferAlignment;
    customStreamInterface *meta_info;
};

bool Interface::RegisterStream( const char *typeName, size_t memBufSize, size_t memBufAlignment, customStreamInterface *streamInterface )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    bool registerSuccess = false;

    // Get the stream environment first.
    streamSystemPlugin *streamSysEnv = streamSystemPluginRegister.get().GetPluginStruct( (EngineInterface*)this );

    if ( streamSysEnv )
    {
        // We need a special type descriptor, just like in the native texture registration.

        scoped_rwlock_writer <rwlock> addNewStreamType( streamSysEnv->streamEnvLock );

        try
        {
            // Attempt to register it into the system.
            RwTypeSystem::typeInfoBase *customTypeInfo =
                engineInterface->typeSystem.RegisterCommonTypeInterface <streamCustomTypeInterface> (
                    typeName, engineInterface->streamTypeInfo, memBufSize, memBufAlignment, streamInterface
                );

            if ( customTypeInfo )
            {
                // We have successfully registered our type, so now we must register it into the system.
                streamSysEnv->custom_types.AddToBack( customTypeInfo );
                    
                // Success!
                registerSuccess = true;
            }
        }
        catch( ... )
        {
            registerSuccess = false;

            // We do not rethrow, as we simply return a boolean.
        }
    }

    return registerSuccess;
}

Stream* Interface::CreateStream( eBuiltinStreamType streamType, eStreamMode streamMode, streamConstructionParam_t *param )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    Stream *outputStream = nullptr;

    streamSystemPlugin *streamSysEnv = streamSystemPluginRegister.get().GetPluginStruct( (EngineInterface*)this );

    if ( streamSysEnv )
    {
        scoped_rwlock_reader <rwlock> streamSysConsistency( streamSysEnv->streamEnvLock );

        if ( streamType == RWSTREAMTYPE_FILE || streamType == RWSTREAMTYPE_FILE_W )
        {
            FileInterface::filePtr_t fileHandle = nullptr;

            // Only proceed if we have a file stream type.
            if ( RwTypeSystem::typeInfoBase *fileStreamTypeInfo = streamSysEnv->fileStreamTypeInfo )
            {
                if ( streamType == RWSTREAMTYPE_FILE )
                {
                    if ( param->dwSize >= sizeof( streamConstructionFileParam_t ) )
                    {
                        streamConstructionFileParam_t *file_param = (streamConstructionFileParam_t*)param;

                        // Dispatch the given stream mode into an ANSI descriptor.
                        const char *ansi_mode = "";

                        if ( streamMode == RWSTREAMMODE_READONLY )
                        {
                            ansi_mode = "rb";
                        }
                        else if ( streamMode == RWSTREAMMODE_READWRITE )
                        {
                            ansi_mode = "rb+";
                        }
                        else if ( streamMode == RWSTREAMMODE_WRITEONLY )
                        {
                            ansi_mode = "wb";
                        }
                        else if ( streamMode == RWSTREAMMODE_CREATE )
                        {
                            ansi_mode = "wb+";
                        }

                        fileHandle = this->GetFileInterface()->OpenStream( file_param->filename, ansi_mode );
                    }
                }
                else if ( streamType == RWSTREAMTYPE_FILE_W )
                {
                    if ( param->dwSize >= sizeof( streamConstructionFileParamW_t ) )
                    {
                        streamConstructionFileParamW_t *file_param = (streamConstructionFileParamW_t*)param;

                        // Dispatch the given stream mode into an ANSI descriptor.
                        const wchar_t *ansi_mode = L"";

                        if ( streamMode == RWSTREAMMODE_READONLY )
                        {
                            ansi_mode = L"rb";
                        }
                        else if ( streamMode == RWSTREAMMODE_READWRITE )
                        {
                            ansi_mode = L"rb+";
                        }
                        else if ( streamMode == RWSTREAMMODE_WRITEONLY )
                        {
                            ansi_mode = L"wb";
                        }
                        else if ( streamMode == RWSTREAMMODE_CREATE )
                        {
                            ansi_mode = L"wb+";
                        }

                        fileHandle = this->GetFileInterface()->OpenStreamW( file_param->filename, ansi_mode );
                    }
                }

                if ( fileHandle )
                {
                    GenericRTTI *rttiObj = engineInterface->typeSystem.Construct( engineInterface, fileStreamTypeInfo, nullptr );

                    if ( rttiObj )
                    {
                        FileStream *fileStream = (FileStream*)RwTypeSystem::GetObjectFromTypeStruct( rttiObj );

                        // Give the file handle to the stream.
                        // It will take care of freeing that handle.
                        fileStream->file_handle = fileHandle;

                        outputStream = fileStream;
                    }

                    if ( outputStream == nullptr )
                    {
                        this->GetFileInterface()->CloseStream( fileHandle );
                    }
                }
            }
        }
        else if ( streamType == RWSTREAMTYPE_MEMORY )
        {
            // Decide if we need a constant-memory stream or a growing-memory stream.
            if ( param->dwSize >= sizeof(streamConstructionMemoryParam_t) )
            {
                const streamConstructionMemoryParam_t *memParams = (const streamConstructionMemoryParam_t*)param;

                void *bufPtr = memParams->buf;

                if ( bufPtr != nullptr )
                {
                    // Constant-size memory buffer stream.
                    if ( RwTypeSystem::typeInfoBase *memStreamTypeInfo = streamSysEnv->memoryStreamTypeInfo )
                    {
                        FixedBufferMemoryStream::constrParams nativeParams;
                        nativeParams.buf = bufPtr;
                        nativeParams.bufSize = memParams->bufSize;
                        nativeParams.streamMode = streamMode;

                        GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, memStreamTypeInfo, &nativeParams );

                        if ( rtObj )
                        {
                            // Success!
                            FixedBufferMemoryStream *memStream = (FixedBufferMemoryStream*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );

                            outputStream = memStream;
                        }
                    }
                }
                else
                {
                    // Expansive memory stream.
                    if ( RwTypeSystem::typeInfoBase *memexpandStreamTypeInfo = streamSysEnv->memexpandStreamTypeInfo )
                    {
                        DynamicBufferMemoryStream::constrParams nativeParams;
                        nativeParams.streamMode = streamMode;

                        GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, memexpandStreamTypeInfo, &nativeParams );

                        if ( rtObj )
                        {
                            DynamicBufferMemoryStream *memStream = (DynamicBufferMemoryStream*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );

                            outputStream = memStream;
                        }
                    }
                }
            }
        }
        else if ( streamType == RWSTREAMTYPE_CUSTOM )
        {
            // We need to get the stream type info to proceed.
            if ( RwTypeSystem::typeInfoBase *streamTypeInfo = engineInterface->streamTypeInfo )
            {
                if ( param->dwSize >= sizeof( streamConstructionCustomParam_t ) )
                {
                    streamConstructionCustomParam_t *customParam = (streamConstructionCustomParam_t*)param;

                    // Attempt construction with the custom param.
                    // Since we cannot do too much verification here, we hope that the vendor does things right.

                    // Get the type info associated.
                    if ( RwTypeSystem::typeInfoBase *customTypeInfo = engineInterface->typeSystem.FindTypeInfo( customParam->typeName, streamTypeInfo ) )
                    {
                        // We need to fetch our custom type information struct.
                        // If we cannot resolve it, the type info is not a valid custom stream type info.
                        streamCustomTypeInterface *typeProvider = dynamic_cast <streamCustomTypeInterface*> ( customTypeInfo->tInterface );

                        if ( typeProvider )
                        {
                            customStreamConstructionParams cstr_params;
                            cstr_params.streamMode = streamMode;
                            cstr_params.userdata = customParam->userdata;

                            // Construct the object.
                            GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, customTypeInfo, &cstr_params );

                            if ( rtObj )
                            {
                                // We are successful!
                                CustomStream *customStream = (CustomStream*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );

                                // Give the functionality provider to the stream.
                                customStream->streamProvider = typeProvider->meta_info;

                                outputStream = customStream;
                            }
                        }
                    }
                }
            }
        }
    }

    return outputStream;
}

void Interface::DeleteStream( Stream *theStream )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    // Just rek it.
    engineInterface->typeSystem.Destroy( engineInterface, RwTypeSystem::GetTypeStructFromObject( theStream ) );
}

void registerStreamGlobalPlugins( void )
{
    streamSystemPluginRegister.Construct( engineFactory );
}

void unregisterStreamGlobalPlugins( void )
{
    streamSystemPluginRegister.Destroy();
}

};