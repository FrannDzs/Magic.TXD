/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.vfs.h
*  PURPOSE:     Virtual filesystem translator for filesystem emulation
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_VFS_IMPLEMENTATION_
#define _FILESYSTEM_VFS_IMPLEMENTATION_

// For internalFilePath.
#include "CFileSystem.internal.common.h"

#include "CFileSystem.Utils.hxx"

#include "CFileSystem.translator.scanutil.hxx"

#include <sdk/Vector.h>
#include <sdk/String.h>
#include <sdk/GlobPattern.h>
#include <sdk/AVLTree.h>
#include <sdk/UniChar.h>

// This class implements a virtual file system.
// It consists of nodes which are either directories or files.
// Each, either directory or file, has its own set of meta data.

// CURRENT PROPERTIES OF THIS VIRTUAL FILE SYSTEM TREE:
// * directory and file of same name are allowed to exist in the same parent dir
// * there is only one name-link to each file/directory (unlike NTFS)

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4250)
#endif //_MSC_VER

namespace VirtualFileSystem
{
    // These types should be used in a translator that uses CVirtualFileSystem

    struct directoryInterface;

    struct fsObjectInterface abstract
    {
        virtual void*                       GetManager      ( void ) const noexcept = 0;
        virtual directoryInterface*         GetParent       ( void ) noexcept = 0;
        virtual const internalFilePath&     GetRelativePath ( void ) const noexcept = 0;
        virtual fsOffsetNumber_t            GetDataSize     ( void ) const noexcept = 0;
        virtual bool                        IsLocked        ( void ) const noexcept = 0;
    };

    struct directoryInterface abstract : public virtual fsObjectInterface
    {
        virtual bool IsEmpty( void ) const noexcept = 0;
    };

    struct fileInterface abstract : public virtual fsObjectInterface
    {
        // Nothing.
    };
};

enum class eVFSFileOpenMode
{
    OPEN,
    CREATE
};

#define CVFS_TEMPLARGS \
template <typename translatorType, typename directoryMetaData, typename fileMetaData>
#define CVFS_TEMPLUSE \
<translatorType, directoryMetaData, fileMetaData>

CVFS_TEMPLARGS
struct CVirtualFileSystem
{
    // REQUIREMENTS FOR translatorType TYPE:
    // * has to inherit from CFileTranslator
    // * void OnRemoveRemoteFile( const dirNames& treeToFile, const internalFilePath& thePath );
    // * CFile* OpenNativeFileStream( VirtualFileSystem::fileInterface *theFile, eVFSFileOpenMode openMode, const filesysAccessFlags& access );
    // REQUIREMENTS FOR directoryMetaData TYPE:
    // * constructor( translatorType *hostTrans, VirtualFileSystem::directoryInterface *intf );
    // REQUIREMENTS FOR fileMetaData TYPE:
    // * constructor( translatorType *hostTrans, VirtualFileSystem::fileInterface *intf );
    // * void Reset( void );
    // * fsOffsetNumber_t GetSize( void ) const;
    // * bool IsLocked( void ) const;
    // * void OnFileDelete( void );
    // * bool OnFileRename( const dirNames& treeToFile, const internalFilePath& fileName );
    // * bool OnFileCopy( const dirNames& treeToFile, const internalFilePath& fileName );
    // * bool CopyAttributesTo( fileMetaData& copyTo );
    // * void GetANSITimes( time_t& mtime, time_t& ctime, time_t& atime ) const;
    // * void GetDeviceIdentifier( dev_t& idOut ) const;

    CVirtualFileSystem( bool caseSensitive ) : m_rootDir( this, nullptr, internalFilePath(), internalFilePath(), 0, false )
    {
        // We zero it out for debugging.
        this->hostTranslator = nullptr;

        // Current directory starts at root
        m_curDirEntry = &m_rootDir;

        this->pathCaseSensitive = caseSensitive;
    }

    ~CVirtualFileSystem( void )
    {
        return;
    }

    // Host translator of this VFS.
    // Needs to be set by the translator that uses this VFS.
    translatorType *hostTranslator;

private:
    // Decides whether string comparison is done case-sensitively.
    // Tree has to be rebuilt if this option changes.
    bool pathCaseSensitive;

public:
    // Forward declarations.
    struct file;
    struct directory;

    struct fsActiveEntry : virtual public VirtualFileSystem::fsObjectInterface
    {
        // Implemented later.
        fsActiveEntry( CVirtualFileSystem *manager, directory *parentDir, internalFilePath name, internalFilePath relPath, size_t ordering, bool hasOrdering );
        ~fsActiveEntry( void );

        VirtualFileSystem::directoryInterface* GetParent( void ) noexcept override
        {
            return this->parentDir;
        }

        const internalFilePath& GetRelativePath( void ) const noexcept override
        {
            return relPath;
        }

        fsOffsetNumber_t GetDataSize( void ) const noexcept override
        {
            return 0;
        }

        void* GetManager( void ) const noexcept override
        {
            return this->manager;
        }

        // TODO: refactor this code so that filesystem items can have multiple names (hard links).

        internalFilePath        name;
        internalFilePath        relPath;

        AVLNode                 dirNode;
        directory*              parentDir;  // a directory is the sole parent-entity.

        CVirtualFileSystem*     manager;

        // When data is parsed from binary sources then we often want to preserve idem-potence of data.
        // Thus preserving an order of things is much desired.
        AVLNode                 orderByNode;
        size_t                  orderBy;
        bool                    hasOrderBy;

        bool isDirectory, isFile;
    };

    struct file final : public fsActiveEntry, public VirtualFileSystem::fileInterface
    {
        file(
            CVirtualFileSystem *manager,
            directory *parentDir,
            internalFilePath name, internalFilePath relPath,
            size_t orderBy, bool hasOrderBy
        ) : fsActiveEntry( manager, parentDir, std::move( name ), std::move( relPath ), orderBy, hasOrderBy ),
            metaData( manager->hostTranslator, this )
        {
            this->isFile = true;
        }

        file( const file& right ) = delete;
        file( file&& right ) = delete;

        ~file( void )       {}

        file& operator = ( const file& right ) = delete;
        file& operator = ( file&& right ) = delete;

        inline void OnRecreation( void )
        {
            this->metaData.Reset();
        }

        fsOffsetNumber_t GetDataSize( void ) const noexcept override
        {
            return this->metaData.GetSize();
        }

        bool IsLocked( void ) const noexcept override
        {
            return this->metaData.IsLocked();
        }

        fileMetaData    metaData;
    };

    struct directory final : public fsActiveEntry, public VirtualFileSystem::directoryInterface
    {
        inline directory(
            CVirtualFileSystem *manager,
            directory *parentDir,
            internalFilePath fileName, internalFilePath path,
            size_t orderBy, bool hasOrderBy
        ) : fsActiveEntry( manager, parentDir, std::move( fileName ), std::move( path ), orderBy, hasOrderBy ),
            metaData( manager->hostTranslator, this )
        {
            this->isDirectory = true;
        }

    private:
        inline void deleteDirectory( directory *theDir )
        {
            eir::static_del_struct <directory, FSObjectHeapAllocator> ( nullptr, theDir );
        }

        inline void deleteFile( file *theFile )
        {
            eir::static_del_struct <file, FSObjectHeapAllocator> ( nullptr, theFile );
        }

    public:
        inline ~directory( void )
        {
            // Until we have root destroy all items.
            while ( AVLNode *node = this->fsItemSortedByNameTree.GetRootNode() )
            {
                const fsActiveEntry *item = AVL_GETITEM( fsActiveEntry, node, dirNode );

                if ( item->isFile )
                {
                    deleteFile( (file*)item );
                }
                else if ( item->isDirectory )
                {
                    deleteDirectory( (directory*)item );
                }
            }
        }

        fsOffsetNumber_t GetDataSize( void ) const noexcept override
        {
            fsOffsetNumber_t theDataSize = 0;

            this->fsItemSortedByNameTree.Walk(
                [&]( const AVLNode *node )
            {
                const fsActiveEntry *item = AVL_GETITEM( fsActiveEntry, node, dirNode );

                theDataSize += item->GetDataSize();
            });

            return theDataSize;
        }

        struct fsNodeSortByNameDispatcher
        {
            static AINLINE eir::eCompResult CompareNodes( const AVLNode *left, const AVLNode *right )
            {
                const fsActiveEntry *leftEntry = AVL_GETITEM( fsActiveEntry, left, dirNode );
                const fsActiveEntry *rightEntry = AVL_GETITEM( fsActiveEntry, right, dirNode );

                bool caseSensitive = leftEntry->manager->pathCaseSensitive;

                return ( leftEntry->name.compare( rightEntry->name, caseSensitive ) );
            }

            template <typename allocatorType>
            static AINLINE eir::eCompResult CompareNodeWithValue( const AVLNode *left, const eir::MultiString <allocatorType>& right )
            {
                const fsActiveEntry *leftEntry = AVL_GETITEM( fsActiveEntry, left, dirNode );

                bool caseSensitive = leftEntry->manager->pathCaseSensitive;

                return ( leftEntry->name.compare( right, caseSensitive ) );
            }
        };

        mutable AVLTree <fsNodeSortByNameDispatcher> fsItemSortedByNameTree;

        directoryMetaData metaData;

        template <typename allocatorType>
        inline const fsActiveEntry* FindAnyChildObject( const eir::MultiString <allocatorType>& dirName ) const
        {
            const AVLNode *foundNode = this->fsItemSortedByNameTree.FindNode( dirName );

            if ( !foundNode )
            {
                return nullptr;
            }

            const fsActiveEntry *foundItem = AVL_GETITEM( fsActiveEntry, foundNode, dirNode );

            return foundItem;
        }

        template <typename allocatorType>
        inline fsActiveEntry* FindAnyChildObject( const eir::MultiString <allocatorType>& dirName )
        {
            AVLNode *foundNode = this->fsItemSortedByNameTree.FindNode( dirName );

            if ( !foundNode )
                return nullptr;

            fsActiveEntry *foundItem = AVL_GETITEM( fsActiveEntry, foundNode, dirNode );

            return foundItem;
        }

        template <typename allocatorType>
        inline const directory*  FindDirectory( const eir::MultiString <allocatorType>& dirName ) const
        {
            AVLNode *foundNode = this->fsItemSortedByNameTree.FindNode( dirName );

            if ( !foundNode )
                return nullptr;

            typename decltype(this->fsItemSortedByNameTree)::nodestack_iterator ns_iter( foundNode );

            while ( ns_iter.IsEnd() == false )
            {
                const fsActiveEntry *item = AVL_GETITEM( fsActiveEntry, ns_iter.Resolve(), dirNode );

                if ( item->isDirectory )
                {
                    return (const directory*)item;
                }

                ns_iter.Increment();
            }

            return nullptr;
        }

        template <typename allocatorType>
        inline const file*  FindFile( const eir::MultiString <allocatorType>& fileName ) const
        {
            AVLNode *foundNode = this->fsItemSortedByNameTree.FindNode( fileName );

            if ( !foundNode )
                return nullptr;

            typename decltype(this->fsItemSortedByNameTree)::nodestack_iterator ns_iter( foundNode );

            while ( ns_iter.IsEnd() == false )
            {
                const fsActiveEntry *item = AVL_GETITEM( fsActiveEntry, ns_iter.Resolve(), dirNode );

                if ( item->isFile )
                {
                    return (const file*)item;
                }

                ns_iter.Increment();
            }

            return nullptr;
        }

        template <typename allocatorType>
        inline directory*  FindDirectory( const eir::MultiString <allocatorType>& dirName )
        {
            AVLNode *foundNode = this->fsItemSortedByNameTree.FindNode( dirName );

            if ( !foundNode )
                return nullptr;

            typename decltype(this->fsItemSortedByNameTree)::nodestack_iterator ns_iter( foundNode );

            while ( ns_iter.IsEnd() == false )
            {
                fsActiveEntry *item = AVL_GETITEM( fsActiveEntry, ns_iter.Resolve(), dirNode );

                if ( item->isDirectory )
                {
                    return (directory*)item;
                }

                ns_iter.Increment();
            }

            return nullptr;
        }

        template <typename allocatorType>
        inline file*  FindFile( const eir::MultiString <allocatorType>& fileName )
        {
            AVLNode *foundNode = this->fsItemSortedByNameTree.FindNode( fileName );

            if ( !foundNode )
                return nullptr;

            typename decltype(this->fsItemSortedByNameTree)::nodestack_iterator ns_iter( foundNode );

            while ( ns_iter.IsEnd() == false )
            {
                fsActiveEntry *item = AVL_GETITEM( fsActiveEntry, ns_iter.Resolve(), dirNode );

                if ( item->isFile )
                {
                    return (file*)item;
                }

                ns_iter.Increment();
            }

            return nullptr;
        }

private:
        inline internalFilePath CalculatePositionOfChildObject( const internalFilePath& nameOfItem, bool isDirectory )
        {
            internalFilePath outObjPath = this->relPath;
            outObjPath += nameOfItem;

            if ( isDirectory )
            {
                outObjPath += FileSystem::GetDefaultDirectorySeparator <char> ();
            }

            return outObjPath;
        }

public:
        inline directory&  GetDirectory( internalFilePath dirName, size_t ordering, bool hasOrdering )
        {
            if ( directory *dir = FindDirectory( dirName ) )
            {
                return *dir;
            }

            internalFilePath relPathToDir = CalculatePositionOfChildObject( dirName, true );

            directory *dir = eir::static_new_struct <directory, FSObjectHeapAllocator> ( nullptr, this->manager, this, std::move( dirName ), std::move( relPathToDir ), ordering, hasOrdering );

            return *dir;
        }

        inline file&    AddFile( internalFilePath fileName, size_t ordering, bool hasOrdering )
        {
            internalFilePath relPathToFile = CalculatePositionOfChildObject( fileName, false );

            file *entry = eir::static_new_struct <file, FSObjectHeapAllocator> ( nullptr, this->manager, this, std::move( fileName ), std::move( relPathToFile ), ordering, hasOrdering );

            return *entry;
        }

        inline void MoveTo( fsActiveEntry& entry, internalFilePath newName )
        {
            // Make sure a file or directory named like this does not exist in this directory already!
            // You have to do it
            assert( this->fsItemSortedByNameTree.FindNode( newName ) == nullptr );

            bool isDirectory = entry.isDirectory;

            internalFilePath newObjPos = this->CalculatePositionOfChildObject( newName, isDirectory );

            // Remove from the old tree and put into the new one.
            if ( directory *oldParent = entry.parentDir )
            {
                oldParent->fsItemSortedByNameTree.RemoveByNodeFast( &entry.dirNode );
            }

            // Update properties.
            entry.name = std::move( newName );

            this->fsItemSortedByNameTree.Insert( &entry.dirNode );

            // Update parent relationship.
            entry.parentDir = this;

            entry.relPath = std::move( newObjPos );
        }

        bool IsLocked( void ) const noexcept override
        {
            typename decltype(this->fsItemSortedByNameTree)::diff_node_iterator iter( this->fsItemSortedByNameTree.GetSmallestNode() );

            // Check all children whether they are locked.
            // If any of it is locked, then the parent is locked too.
            while ( !iter.IsEnd() )
            {
                typename decltype(this->fsItemSortedByNameTree)::nodestack_iterator ns_iter( iter.Resolve() );

                while ( !ns_iter.IsEnd() )
                {
                    const AVLNode *node = ns_iter.Resolve();

                    const fsActiveEntry *item = AVL_GETITEM( fsActiveEntry, node, dirNode );

                    if ( item->IsLocked() )
                    {
                        return true;
                    }

                    ns_iter.Increment();
                }

                iter.Increment();
            }

            return false;
        }

        inline file*    MakeFile( internalFilePath fileName, size_t ordering, bool hasOrdering )
        {
            file *entry = FindFile( fileName );

            if ( entry )
            {
                if ( entry->metaData.IsLocked() )
                    return nullptr;

                CVirtualFileSystem *manager = this->manager;

                // Update the meta-data.
                this->fsItemSortedByNameTree.RemoveByNodeFast( &entry->dirNode );
                manager->fsItemSortedByOrderTree.RemoveByNodeFast( &entry->orderByNode );

                entry->name = std::move( fileName );
                entry->orderBy = ordering;
                entry->hasOrderBy = hasOrdering;

                this->fsItemSortedByNameTree.Insert( &entry->dirNode );
                manager->fsItemSortedByOrderTree.Insert( &entry->orderByNode );

                entry->OnRecreation();
                return entry;
            }

            return &AddFile( std::move( fileName ), ordering, hasOrdering );
        }

        inline bool RemoveObject( fsActiveEntry *object )
        {
            if ( object->IsLocked() )
                return false;

            if ( object->isFile )
            {
                file *theFile = (file*)object;

                theFile->metaData.OnFileDelete();

                deleteFile( theFile );
                return true;
            }
            if ( object->isDirectory )
            {
                directory *theDir = (directory*)object;

                deleteDirectory( theDir );
                return true;
            }

            return false;
        }

        template <typename allocatorType>
        inline bool     RemoveObjectByName( const eir::MultiString <allocatorType>& fileName, bool removeFile = true, bool removeDir = true )
        {
            fsActiveEntry *theObject = this->FindChildObject( fileName );

            if ( !theObject )
                return false;

            if ( theObject->isFile )
            {
                if ( removeFile == false )
                    return false;
            }
            else if ( theObject->isDirectory )
            {
                if ( removeDir == false )
                    return false;
            }

            return RemoveObject( theObject );
        }

        inline bool     IsEmpty( void ) const noexcept override
        {
            return ( this->fsItemSortedByNameTree.GetRootNode() == nullptr );
        }

        // Returns true if this node is inside of the tree of theDir.
        inline bool InheritsFrom( const directory *theDir ) const
        {
            directory *parent = this->parentDir;

            while ( parent )
            {
                if ( parent == theDir )
                    return true;

                parent = parent->parentDir;
            }

            return false;
        }

        // Helper functions exposed for the runtimes.
        template <typename callbackType>
        inline void ForAllChildrenByName( const callbackType& cb )
        {
            typename decltype(this->fsItemSortedByNameTree)::diff_node_iterator iter( this->fsItemSortedByNameTree );

            while ( !iter.IsEnd() )
            {
                typename decltype(this->fsItemSortedByNameTree)::nodestack_iterator ns_iter( iter.Resolve() );

                while ( !ns_iter.IsEnd() )
                {
                    fsActiveEntry *item = AVL_GETITEM( fsActiveEntry, ns_iter.Resolve(), dirNode );

                    cb( item );

                    ns_iter.Increment();
                }

                iter.Increment();
            }
        }
    };

    struct fsNodeSortByOrderDispatcher
    {
        static AINLINE eir::eCompResult compare_values(
            const size_t& leftOrderBy, bool leftHasOrder,
            const size_t& rightOrderBy, bool rightHasOrder
        )
        {
            if ( leftHasOrder && rightHasOrder )
            {
                if ( leftOrderBy < rightOrderBy )
                {
                    return eir::eCompResult::LEFT_LESS;
                }

                if ( leftOrderBy > rightOrderBy )
                {
                    return eir::eCompResult::LEFT_GREATER;
                }

                return eir::eCompResult::EQUAL;
            }

            // If an item has no order, then it gets virtually added to a last node
            // that contains all items which have no order (a DONT_CARE node).
            // So anything that has an order is smaller than it, to come before.
            if ( leftHasOrder )
            {
                return eir::eCompResult::LEFT_LESS;
            }

            if ( rightHasOrder )
            {
                return eir::eCompResult::LEFT_GREATER;
            }

            return eir::eCompResult::EQUAL;
        }

        static AINLINE eir::eCompResult CompareNodes( const AVLNode *left, const AVLNode *right )
        {
            const fsActiveEntry *leftEntry = AVL_GETITEM( fsActiveEntry, left, orderByNode );
            const fsActiveEntry *rightEntry = AVL_GETITEM( fsActiveEntry, right, orderByNode );

            return (
                compare_values( leftEntry->orderBy, leftEntry->hasOrderBy, rightEntry->orderBy, rightEntry->hasOrderBy )
            );
        }

        static AINLINE eir::eCompResult CompareNodeWithValue( const AVLNode *left, size_t rightOrderBy )
        {
            const fsActiveEntry *leftEntry = AVL_GETITEM( fsActiveEntry, left, orderByNode );

            return compare_values( leftEntry->orderBy, leftEntry->hasOrderBy, rightOrderBy, true );
        }
    };

    // Tree of all items sorted in serialization-order.
    mutable AVLTree <fsNodeSortByOrderDispatcher, false> fsItemSortedByOrderTree;

    // Root node of the tree.
    directory m_rootDir;

    inline directory&   GetRootDir( void )
    {
        return m_rootDir;
    }

    // Current virtual directory.
    directory*  m_curDirEntry;

    inline directory*   GetCurrentDir( void )
    {
        return m_curDirEntry;
    }

    // Helper methods for node traversal on a virtual file system.
    inline const fsActiveEntry* GetObjectNode( const char *path, filesysPathOperatingMode opMode = filesysPathOperatingMode::DEFAULT ) const
    {
        normalNodePath normalPath;

        CFileTranslator *hostTranslator = this->hostTranslator;

        if ( !hostTranslator->GetRelativePathNodes( path, normalPath ) )
            return nullptr;

        bool isNormalPathFilepath = normalPath.isFilePath;

        filePath fileName;

        if ( isNormalPathFilepath )
        {
            fileName = std::move( normalPath.travelNodes.GetBack() );
            normalPath.travelNodes.RemoveFromBack();
        }

        const directory *dir = GetDeriviateDir( *m_curDirEntry, normalPath );

        // If we could not find the hosting directory then we give up here.
        if ( dir == nullptr )
        {
            return nullptr;
        }

        // Fetch the filesystem item.
        const fsActiveEntry *theEntry = nullptr;

        if ( opMode == filesysPathOperatingMode::DEFAULT )
        {
            filesysPathProcessMode procMode = hostTranslator->GetPathProcessMode();

            if ( procMode == filesysPathProcessMode::DISTINGUISHED )
            {
                if ( isNormalPathFilepath )
                {
                    theEntry = dir->FindFile( fileName );
                }
                else
                {
                    theEntry = dir;
                }
            }
            else if ( procMode == filesysPathProcessMode::AMBIVALENT_FILE )
            {
                if ( isNormalPathFilepath )
                {
                    theEntry = dir->FindAnyChildObject( fileName );
                }
                else
                {
                    theEntry = dir;
                }
            }
            else
            {
#ifdef _DEBUG
                assert( 0 );
#endif //_DEBUG

                throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
            }
        }
        else if ( opMode == filesysPathOperatingMode::DIR_ONLY )
        {
            if ( isNormalPathFilepath )
            {
                theEntry = dir->FindDirectory( fileName );
            }
            else
            {
                theEntry = dir;
            }
        }
        else if ( opMode == filesysPathOperatingMode::FILE_ONLY )
        {
            if ( isNormalPathFilepath )
            {
                theEntry = dir->FindFile( fileName );
            }
        }

        return theEntry;
    }

    inline const directory* GetDirTree( const directory& root, const dirNames& relTree, size_t iter, size_t end ) const
    {
        const directory *curDir = &root;

        for ( ; iter < end; ++iter )
        {
            const filePath& nameItem = relTree[ iter ];

            if ( !( curDir = curDir->FindDirectory( nameItem ) ) )
            {
                return nullptr;
            }
        }

        return curDir;
    }

    inline const directory* GetDirTree( const dirNames& tree ) const
    {
        return GetDirTree( this->m_rootDir, tree, 0, tree.GetCount() );
    }

    // For getting a directory.
    inline const directory* GetDeriviateDir( const directory& root, const normalNodePath& nodePath ) const
    {
        const directory *curDir = &root;
        size_t backCount = nodePath.backCount;
        
        for ( size_t n = 0; n < backCount; n++ )
        {
            curDir = curDir->parentDir;

            if ( !curDir )
                return nullptr;
        }

        return GetDirTree( *curDir, nodePath.travelNodes, 0, nodePath.travelNodes.GetCount() );
    }

    // Common-purpose directory creation function.
    // Ignores the isFilePath property of nodePath.
    inline directory* MakeDeriviateDir( directory& root, const normalNodePath& nodePath, bool createMissingDirs, bool createParentDirs, bool lastEntryIsEmptyDirectory, bool hasOrderBy = false, size_t orderBy = 0 )
    {
        // If lastEntryIsEmptyDirectory is true, then the case is just the same like treating nodePath as full of parent directories.

        directory *curDir = &root;
        size_t backCount = nodePath.backCount;
        
        for ( size_t n = 0; n < backCount; n++ )
        {
            curDir = curDir->parentDir;

            if ( !curDir )
                return nullptr;
        }

        size_t count = nodePath.travelNodes.GetCount();

        if ( count > 0 )
        {
            size_t numParentDirs;

            if ( lastEntryIsEmptyDirectory )
            {
                numParentDirs = count;
            }
            else
            {
                numParentDirs = ( count - 1 );
            }

            for ( size_t n = 0; n < count; n++ )
            {
                const filePath& thePathToken = nodePath.travelNodes[ n ];

                if ( createMissingDirs && ( createParentDirs || n >= numParentDirs ) )
                {
                    curDir = &curDir->GetDirectory( thePathToken, orderBy, hasOrderBy );
                }
                else
                {
                    curDir = curDir->FindDirectory( thePathToken );

                    if ( curDir == nullptr )
                    {
                        return nullptr;
                    }
                }
            }
        }

        return curDir;
    }

    // USE THIS FUNCTION ONLY IF YOU KNOW THAT tree IS AN ABSOLUTE PATH!
    inline directory& _CreateDirTree( directory& dir, const dirNames& tree, size_t orderBy, bool hasOrderBy )
    {
        directory *curDir = &dir;
        size_t iter = 0;
        size_t count = tree.GetCount();

        for ( ; iter < count; ++iter )
        {
            const filePath& thePath = tree[ iter ];

            curDir = &curDir->GetDirectory( thePath, orderBy, hasOrderBy );
        }

        return *curDir;
    }

    // Making your life easier!
    // fileName is just the name of the file itself, not a path.
    inline file* ConstructFile( const normalNodePath& dirOfFile, internalFilePath fileName, bool createParentDirs, size_t ordering, bool hasOrdering )
    {
        directory *targetDir = MakeDeriviateDir( *m_curDirEntry, dirOfFile, createParentDirs, createParentDirs, true, hasOrdering, ordering );

        if ( targetDir )
        {
            return targetDir->MakeFile( std::move( fileName ), ordering, hasOrdering );
        }

        return nullptr;
    }

private:
    template <typename allocatorType>
    inline file* BrowseFile( const normalNodePath& dirOfFile, const eir::MultiString <allocatorType>& fileName )
    {
        directory *dir = (directory*)GetDeriviateDir( *m_curDirEntry, dirOfFile );

        if ( dir )
        {
            return dir->FindFile( fileName );
        }

        return nullptr;
    }

public:
    inline bool RenameObject( internalFilePath newName, const normalNodePath& rootPath, fsActiveEntry *object )
    {
        if ( object->IsLocked() )
            return false;

        // Get the directory where to put the object.
        directory *targetDir = this->MakeDeriviateDir( this->m_rootDir, rootPath, true, true, true );

        if ( targetDir )
        {
            // Directory or file?
            if ( object->isFile )
            {
                file *fileEntry = (file*)object;

                // Make sure a file with that name does not exist already.
                if ( targetDir->FindFile( newName ) == nullptr )
                {
                    // First move all kinds of meta data.
                    bool metaMoveSuccess = fileEntry->metaData.OnFileRename( rootPath.travelNodes, newName );

                    if ( metaMoveSuccess )
                    {
                        goto doAttemptMove;
                    }
                }
            }
            else if ( object->isDirectory )
            {
                directory *dirEntry = (directory*)object;

                // Make sure we do not uproot ourselves.
                if ( !targetDir->InheritsFrom( dirEntry ) )
                {
                    // Make sure a directory with that name does not exist already.
                    if ( targetDir->FindDirectory( newName ) == nullptr )
                    {
                        goto doAttemptMove;
                    }
                }
            }
        }
        return false;

    doAttemptMove:
        // Give it a new name.
        targetDir->MoveTo( *object, std::move( newName ) );
        return true;
    }

    inline bool CopyObject( internalFilePath newName, const normalNodePath& rootPath, fsActiveEntry *object )
    {
        bool copySuccess = false;

        if ( object->isFile )
        {
            file *fileObject = (file*)object;

            // Attempt to copy the data to the new location.
            bool metaDataCopySuccess = fileObject->metaData.OnFileCopy( rootPath.travelNodes, newName );

            if ( metaDataCopySuccess )
            {
                try
                {
                    file *dstEntry = ConstructFile( rootPath, std::move( newName ), true, 0, false );

                    if ( dstEntry )
                    {
                        try
                        {
                            // Copy parameters.
                            bool attribCopySuccess = fileObject->metaData.CopyAttributesTo( dstEntry->metaData );

                            if ( attribCopySuccess )
                            {
                                copySuccess = true;
                            }
                        }
                        catch( ... )
                        {
                            // Get the name of the file back on stack again.
                            newName = dstEntry->name;

                            DeleteObject( dstEntry );

                            throw;
                        }

                        // Delete if a failure happens
                        if ( !copySuccess )
                        {
                            // Get the name of the file back on stack again.
                            newName = dstEntry->name;

                            bool deleteOnFailure = DeleteObject( dstEntry );

                            assert( deleteOnFailure == true );
                        }
                    }

                    // If the file could not be allocated, we have to delete the meta-data.
                    if ( !copySuccess )
                    {
                        this->hostTranslator->OnRemoveRemoteFile( rootPath.travelNodes, newName );
                    }
                }
                catch( ... )
                {
                    // Time to clean up meta-data!
                    this->hostTranslator->OnRemoveRemoteFile( rootPath.travelNodes, newName );

                    throw;
                }

                return copySuccess;
            }
        }
        else if ( object->isDirectory )
        {
            // TODO
        }

        return copySuccess;
    }

    inline bool DeleteObject( fsActiveEntry *object )
    {
        directory *parentDir = object->parentDir;

        // Cannot delete the root.
        if ( !parentDir )
            return false;

        return parentDir->RemoveObject( object );
    }

    inline void QueryStatsObject( const fsActiveEntry *entry, filesysStats& statsOut ) const
    {
        time_t mtime = 0;
        time_t ctime = 0;
        time_t atime = 0;

        if ( entry->isFile )
        {
            file *fileEntry = (file*)entry;

            fileEntry->metaData.GetANSITimes( mtime, ctime, atime );
            //fileEntry->metaData.GetDeviceIdentifier( diskID );
        }

        statsOut.atime = atime;
        statsOut.mtime = mtime;
        statsOut.ctime = ctime;

        eFilesysItemType itemType;

        if ( entry->isFile )
        {
            itemType = eFilesysItemType::FILE;
        }
        else if ( entry->isDirectory )
        {
            itemType = eFilesysItemType::DIRECTORY;
        }
        else
        {
            itemType = eFilesysItemType::UNKNOWN;
        }
        statsOut.attribs.type = itemType;

        // TODO: fill out more of the attributes + share with ScanDir logic.
    }

    // Common purpose translator functions.
    inline bool CreateDir( const char *path, filesysPathOperatingMode opMode, bool createParentDirs )
    {
        normalNodePath normalPath;

        CFileTranslator *hostTranslator = this->hostTranslator;

        if ( !hostTranslator->GetRelativePathNodes( path, normalPath ) )
            return false;

        // Create a series of directory tokens where the last one is to be created-on-demand.
        bool shouldRemoveLastToken = false;
        bool lastEntryIsEmptyDirectory = false;

        if ( opMode == filesysPathOperatingMode::DEFAULT )
        {
            filesysPathProcessMode procMode = hostTranslator->GetPathProcessMode();

            if ( procMode == filesysPathProcessMode::DISTINGUISHED )
            {
                shouldRemoveLastToken = normalPath.isFilePath;
                lastEntryIsEmptyDirectory = false;
            }
            else if ( procMode == filesysPathProcessMode::AMBIVALENT_FILE )
            {
                shouldRemoveLastToken = false;
                lastEntryIsEmptyDirectory = false;
            }
            else
            {
#ifdef _DEBUG
                assert( 0 );
#endif //_DEBUG

                throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
            }
        }
        else if ( opMode == filesysPathOperatingMode::FILE_ONLY )
        {
            shouldRemoveLastToken = false;
            lastEntryIsEmptyDirectory = ( normalPath.isFilePath == false );
        }
        else if ( opMode == filesysPathOperatingMode::DIR_ONLY )
        {
            shouldRemoveLastToken = normalPath.isFilePath;
            lastEntryIsEmptyDirectory = false;
        }
        else
        {
#ifdef _DEBUG
            assert( 0 );
#endif //_DEBUG

            throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
        }

        if ( shouldRemoveLastToken )
        {
            normalPath.travelNodes.RemoveFromBack();
        }

        return ( MakeDeriviateDir( *m_curDirEntry, normalPath, true, createParentDirs, lastEntryIsEmptyDirectory ) != nullptr );
    }

    inline bool Exists( const char *path ) const
    {
        return ( GetObjectNode( path ) != nullptr );
    }

    inline CFile* OpenStream( const char *path, const filesysOpenMode& mode )
    {
        // TODO: add directory support; mainly for being able to lock them.
        normalNodePath normalPath;

        if ( !hostTranslator->GetRelativePathNodes( path, normalPath ) || !normalPath.isFilePath )
        {
            ((CFileSystemNative*)fileSystem)->OnTranslatorOpenFailure( eFileOpenFailure::PATH_OUT_OF_SCOPE );
            return nullptr;
        }

        eFileOpenDisposition openType = mode.openDisposition;

        eVFSFileOpenMode vfsOpenMode;

        file *entry = nullptr;
        bool newFileEntry = false;
        {
            filePath name = std::move( normalPath.travelNodes.GetBack() );
            normalPath.travelNodes.RemoveFromBack();

            if ( openType == eFileOpenDisposition::OPEN_EXISTS )
            {
                entry = BrowseFile( normalPath, name );

                if ( entry == nullptr )
                {
                    ((CFileSystemNative*)fileSystem)->OnTranslatorOpenFailure( eFileOpenFailure::NOT_FOUND );
                    return nullptr;
                }

                vfsOpenMode = eVFSFileOpenMode::OPEN;
            }
            else if ( openType == eFileOpenDisposition::CREATE_NO_OVERWRITE )
            {
                bool createParentDirs = mode.createParentDirs;

                directory *dir = MakeDeriviateDir( *this->m_curDirEntry, normalPath, createParentDirs, createParentDirs, true );

                if ( dir == nullptr )
                {
                    ((CFileSystemNative*)fileSystem)->OnTranslatorOpenFailure( eFileOpenFailure::RESOURCES_EXHAUSTED );
                    return nullptr;
                }

                if ( dir->FindAnyChildObject( name ) != nullptr )
                {
                    ((CFileSystemNative*)fileSystem)->OnTranslatorOpenFailure( eFileOpenFailure::ALREADY_EXISTS );
                    return nullptr;
                }

                entry = &dir->AddFile( std::move( name ), 0, false );

                newFileEntry = true;

                vfsOpenMode = eVFSFileOpenMode::CREATE;
            }
            else if ( openType == eFileOpenDisposition::CREATE_OVERWRITE )
            {
                entry = ConstructFile( normalPath, std::move( name ), mode.createParentDirs, 0, false );

                if ( entry == nullptr )
                {
                    ((CFileSystemNative*)fileSystem)->OnTranslatorOpenFailure( eFileOpenFailure::RESOURCES_EXHAUSTED );
                    return nullptr;
                }

                newFileEntry = true;

                vfsOpenMode = eVFSFileOpenMode::CREATE;
            }
            else if ( openType == eFileOpenDisposition::OPEN_OR_CREATE )
            {
                bool createParentDirs = mode.createParentDirs;

                directory *dir = MakeDeriviateDir( *this->m_curDirEntry, normalPath, createParentDirs, createParentDirs, true );

                if ( dir == nullptr )
                {
                    ((CFileSystemNative*)fileSystem)->OnTranslatorOpenFailure( eFileOpenFailure::RESOURCES_EXHAUSTED );
                    return nullptr;
                }

                if ( fsActiveEntry *existing = dir->FindAnyChildObject( name ) )
                {
                    if ( existing->isFile == false )
                    {
                        ((CFileSystemNative*)fileSystem)->OnTranslatorOpenFailure( eFileOpenFailure::ACCESS_DENIED );
                        return nullptr;
                    }

                    entry = (file*)existing;

                    vfsOpenMode = eVFSFileOpenMode::OPEN;
                }
                else
                {
                    entry = &dir->AddFile( std::move( name ), 0, false );

                    newFileEntry = true;

                    vfsOpenMode = eVFSFileOpenMode::CREATE;
                }
            }
            else
            {
                ((CFileSystemNative*)fileSystem)->OnTranslatorOpenFailure( eFileOpenFailure::UNKNOWN_ERROR );

                // TODO: implement any unknown cases.
                return nullptr;
            }
        }

        CFile *outputFile = hostTranslator->OpenNativeFileStream( entry, vfsOpenMode, mode.access );

        if ( outputFile == nullptr )
        {
            if ( newFileEntry )
            {
                // If we failed to create the destination file, and possibly the output stream, we failed to open the file.
                // Hereby destroy our file entry in the VFS again.
                DeleteObject( entry );
            }

            ((CFileSystemNative*)fileSystem)->OnTranslatorOpenFailure( eFileOpenFailure::RESOURCES_EXHAUSTED );
            return nullptr;
        }

        try
        {
            // Seek it
            outputFile->Seek( 0, mode.seekAtEnd ? SEEK_END : SEEK_SET );
        }
        catch( ... )
        {
            delete outputFile;

            throw;
        }

        return outputFile;
    }

    inline fsOffsetNumber_t Size( const char *path ) const
    {
        // TODO: add directory support.

        const fsActiveEntry *entry = GetObjectNode( path );

        fsOffsetNumber_t preferableSize = 0;

        if ( entry )
        {
            preferableSize = entry->GetDataSize();
        }

        return preferableSize;
    }

    inline bool QueryStats( const char *path, filesysStats& statsOut ) const
    {
        const fsActiveEntry *entry = GetObjectNode( path );

        if ( !entry )
            return false;

        QueryStatsObject( entry, statsOut );
        return true;
    }

    inline bool Rename( const char *src, const char *dst )
    {
        fsActiveEntry *fsEntry = (fsActiveEntry*)GetObjectNode( src );

        if ( !fsEntry )
            return false;

        //todo: maybe check whether rename is possible at all, like in the past
        // for the .zip translator.

        normalNodePath dstNormalPath;

        if ( !hostTranslator->GetRelativePathNodesFromRoot( dst, dstNormalPath ) )
            return false;

        if ( dstNormalPath.travelNodes.GetCount() == 0 )
            return false;

        filePath fileName = std::move( dstNormalPath.travelNodes.GetBack() );
        dstNormalPath.travelNodes.RemoveFromBack();

        // Move the file entry first.
        bool renameSuccess = RenameObject( std::move( fileName ), dstNormalPath, fsEntry );

        return renameSuccess;
    }

    inline bool Copy( const char *src, const char *dst )
    {
        fsActiveEntry *fsSrcEntry = (fsActiveEntry*)GetObjectNode( src );

        if ( !fsSrcEntry )
            return false;

        normalNodePath dstNormalPath;

        if ( !hostTranslator->GetRelativePathNodesFromRoot( dst, dstNormalPath ) )
            return false;

        if ( dstNormalPath.travelNodes.GetCount() == 0 )
            return false;

        // Make sure the given path is the same type as the type of the object.
        if ( fsSrcEntry->isFile != dstNormalPath.isFilePath )
            return false;

        filePath fileName = std::move( dstNormalPath.travelNodes.GetBack() );
        dstNormalPath.travelNodes.RemoveFromBack();

        return CopyObject( std::move( fileName ), dstNormalPath, fsSrcEntry );
    }

    inline bool Delete( const char *path, filesysPathOperatingMode opMode )
    {
        fsActiveEntry *toBeDeleted = (fsActiveEntry*)GetObjectNode( path, opMode );

        if ( !toBeDeleted )
            return false;

        return DeleteObject( toBeDeleted );
    }

    inline bool ChangeDirectory( const dirNames& dirTree )
    {
        directory *dir = (directory*)GetDirTree( dirTree );

        if ( !dir )
            return false;

        m_curDirEntry = dir;
        return true;
    }

    // Loop through all items in this system that are serialized.
    template <typename callbackType>
    AINLINE void ForAllItems( const callbackType& cb )
    {
        typename decltype(this->fsItemSortedByOrderTree)::diff_node_iterator iter( this->fsItemSortedByOrderTree );

        while ( !iter.IsEnd() )
        {
            typename decltype(this->fsItemSortedByOrderTree)::nodestack_iterator ns_iter( iter.Resolve() );

            while ( !ns_iter.IsEnd() )
            {
                fsActiveEntry *item = AVL_GETITEM( fsActiveEntry, ns_iter.Resolve(), orderByNode );

                cb( item );

                ns_iter.Increment();
            }

            iter.Increment();
        }
    }

private:
    // There may be cases where a directory and a file exist with the same name.
    struct vfs_fsitem_iterator
    {
        struct info_data
        {
            fsActiveEntry *fsItem;
            internalFilePath filename;
            bool isDirectory;
            filesysAttributes attribs;
        };

        inline vfs_fsitem_iterator( directory *iterDir )
        {
            this->iterDir = iterDir;
        }

        inline void Rewind( void )
        {
            this->lastNameKey.clear();
            this->sameNameIndex = 0;
            this->hasLastNameKey = false;
            this->hasPresentedCurrentDir = false;
            this->hasPresentedParentDir = false;
        }

        inline bool Next( info_data& dataOut )
        {
            directory *iterDir = this->iterDir;

            if ( this->hasPresentedParentDir == false )
            {
                this->hasPresentedParentDir = true;

                if ( directory *parentDir = iterDir->parentDir )
                {
                    info_data data;
                    data.fsItem = parentDir;
                    data.filename = "..";
                    data.isDirectory = true;
                    data.attribs.type = eFilesysItemType::DIRECTORY;
                    dataOut = std::move( data );
                    return true;
                }
            }

            if ( this->hasPresentedCurrentDir == false )
            {
                this->hasPresentedCurrentDir = true;

                info_data data;
                data.fsItem = iterDir;
                data.filename = ".";
                data.isDirectory = true;
                data.attribs.type = eFilesysItemType::DIRECTORY;
                dataOut = std::move( data );
                return true;
            }

            AVLNode *curNode;

            if ( this->hasLastNameKey == false )
            {
                curNode = iterDir->fsItemSortedByNameTree.GetSmallestNode();
            }
            else
            {
                curNode = iterDir->fsItemSortedByNameTree.GetJustAboveOrEqualNode( this->lastNameKey );
            }

            if ( curNode == nullptr )
            {
                return false;
            }

            // TODO: actually implement this key-based safe iterator inside the Eir SDK because
            // I quite like this weak-bound concept.

            fsActiveEntry *curItem = AVL_GETITEM( fsActiveEntry, curNode, dirNode );

            if ( this->hasLastNameKey == false || curItem->name != this->lastNameKey )
            {
            tryNewNode:
                this->lastNameKey = curItem->name;
                this->hasLastNameKey = true;
                this->sameNameIndex = 0;
            }
            else
            {
                // Since we are at the exact same node, we just skip entries we already seen.
                this->sameNameIndex++;

                size_t cntSkipEntries = this->sameNameIndex;

                typename decltype(directory::fsItemSortedByNameTree)::nodestack_iterator ns_iter( curNode );

                bool hasSkippedAll = false;

                for ( size_t n = 0; n < cntSkipEntries; n++ )
                {
                    ns_iter.Increment();

                    if ( ns_iter.IsEnd() )
                    {
                        hasSkippedAll = true;
                        break;
                    }
                }

                if ( hasSkippedAll )
                {
                    typename decltype(directory::fsItemSortedByNameTree)::diff_node_iterator iter( curNode );

                    iter.Increment();

                    if ( iter.IsEnd() )
                    {
                        return false;
                    }
                    else
                    {
                        curNode = iter.Resolve();
                        curItem = AVL_GETITEM( fsActiveEntry, curNode, dirNode );
                        goto tryNewNode;
                    }
                }
                else
                {
                    curNode = ns_iter.Resolve();
                    curItem = AVL_GETITEM( fsActiveEntry, curNode, dirNode );
                }
            }

            // So now we just return the details of the current item.
            info_data data;
            data.fsItem = curItem;
            data.filename = curItem->name;
            data.isDirectory = curItem->isDirectory;

            // Determine the item type.
            eFilesysItemType itemType;

            if ( curItem->isFile )
            {
                itemType = eFilesysItemType::FILE;
            }
            else if ( curItem->isDirectory )
            {
                itemType = eFilesysItemType::DIRECTORY;
            }
            else
            {
                itemType = eFilesysItemType::UNKNOWN;
            }

            data.attribs.type = itemType;

            // Set the attributes.
            data.attribs.isSystem = false;          // files in archives are not important for the OS
            data.attribs.isHidden = false;          // no files can be hidden (I guess?)
            data.attribs.isTemporary = false;       // no temporary flags in archives
            data.attribs.isJunctionOrLink = false;  // maybe implement this for things that support it?

            dataOut = std::move( data );
            return true;
        }

    private:
        directory *iterDir;
        internalFilePath lastNameKey;
        size_t sameNameIndex = 0;
        bool hasLastNameKey = false;
        bool hasPresentedCurrentDir = false;
        bool hasPresentedParentDir = false;
    };

    template <typename charType>
    static void inline _ScanDirectory(
        const CVirtualFileSystem *trans,
        const eir::PathPatternEnv <charType, FSObjectHeapAllocator>& patternEnv,
        const dirNames& tree,
        directory *dir,
        const typename eir::PathPatternEnv <charType, FSObjectHeapAllocator>::filePattern_t& pattern, bool recurse,
        pathCallback_t dirCallback, pathCallback_t fileCallback, void *userdata )
    {
        // Properties first.
        scanFilteringFlags flags;
        flags.noPatternOnDirs = !( dirCallback && !recurse );
        flags.noSystem = true;
        flags.noHidden = true;
        flags.noTemporary = true;
        flags.noParentDirDesc = true;
        flags.noCurrentDirDesc = true;

        filtered_fsitem_iterator <vfs_fsitem_iterator, eir::PathPatternEnv <charType, FSObjectHeapAllocator>> iter( dir, flags, true );

        typename vfs_fsitem_iterator::info_data data;

        while ( iter.Next( patternEnv, pattern, data ) )
        {
            fsActiveEntry *child = data.fsItem;

            if ( data.isDirectory )
            {
                directory *item = (directory*)child;

                if ( dirCallback )
                {
                    if ( flags.noPatternOnDirs == false || iter.MatchDirectory( patternEnv, pattern, data ) )
                    {
                        // Construct the absolute path.
                        filePath abs_path( FileSystem::GetDefaultDirectorySeparator <charType> () );
                        _File_OutputPathTree( tree, false, true, abs_path );

                        abs_path += data.filename;
                        abs_path += FileSystem::GetDefaultDirectorySeparator <charType> ();

                        dirCallback( abs_path, userdata );
                    }
                }

                if ( recurse )
                {
                    dirNames newTree = tree;
                    newTree.AddToBack( data.filename );

                    _ScanDirectory( trans, patternEnv, newTree, item, pattern, recurse, dirCallback, fileCallback, userdata );
                }
            }
            else if ( fileCallback )
            {
                //file *item = (file*)child;

                // Construct the absolute path.
                filePath abs_path( FileSystem::GetDefaultDirectorySeparator <charType> () );
                _File_OutputPathTree( tree, false, true, abs_path );

                abs_path += data.filename;

                fileCallback( abs_path, userdata );
            }
        }
    }

public:
    void ScanDirectory(
        const char *dirPath,
        const char *wildcard,
        bool recurse,
        pathCallback_t dirCallback, pathCallback_t fileCallback, void *userdata ) const
    {
        normalNodePath normalDirPath;

        CFileTranslator *hostTranslator = this->hostTranslator;

        if ( !hostTranslator->GetRelativePathNodesFromRoot( dirPath, normalDirPath ) )
            return;

        bool canHaveTrailingFile = FileSystem::CanDirectoryPathHaveTrailingFile( hostTranslator->GetPathProcessMode(), filesysPathOperatingMode::DEFAULT );

        if ( normalDirPath.isFilePath && canHaveTrailingFile == false )
        {
            normalDirPath.travelNodes.RemoveFromBack();
        }

        directory *dir = (directory*)GetDirTree( normalDirPath.travelNodes );

        if ( !dir )
            return;

        eir::PathPatternEnv <char, FSObjectHeapAllocator> patternEnv( this->pathCaseSensitive );

        // Create a cached file pattern.
        typename decltype(patternEnv)::filePattern_t pattern = patternEnv.CreatePattern( wildcard );

        _ScanDirectory <char> ( this, patternEnv, normalDirPath.travelNodes, dir, pattern, recurse, dirCallback, fileCallback, userdata );
    }

    CDirectoryIterator* BeginDirectoryListing( const char *dirPath, const char *wildcard, const scanFilteringFlags& filter_flags ) const
    {
        normalNodePath normalPath;

        CFileTranslator *hostTranslator = this->hostTranslator;

        if ( !hostTranslator->GetRelativePathNodesFromRoot( dirPath, normalPath ) )
        {
            return nullptr;
        }

        bool canHaveTrailingFile = FileSystem::CanDirectoryPathHaveTrailingFile( hostTranslator->GetPathProcessMode(), filesysPathOperatingMode::DEFAULT );

        if ( normalPath.isFilePath && canHaveTrailingFile == false )
        {
            normalPath.travelNodes.RemoveFromBack();
        }

        directory *dir = (directory*)GetDirTree( normalPath.travelNodes );

        if ( !dir )
        {
            return nullptr;
        }

        return new CGenericDirectoryIterator <char, vfs_fsitem_iterator> ( this->pathCaseSensitive, dir, filter_flags, wildcard );
    }
};

CVFS_TEMPLARGS
CVirtualFileSystem CVFS_TEMPLUSE::fsActiveEntry::fsActiveEntry(
    CVirtualFileSystem *manager,
    directory *parentDir,
    internalFilePath name, internalFilePath relPath,
    size_t ordering, bool hasOrdering
) :
    name( std::move( name ) ),
    relPath( std::move( relPath ) ),
    orderBy( ordering ),
    hasOrderBy( hasOrdering )
{
    this->isFile = false;
    this->isDirectory = false;
    this->manager = manager;
    this->parentDir = parentDir;

    manager->fsItemSortedByOrderTree.Insert( &this->orderByNode );

    if ( parentDir != nullptr )
    {
        parentDir->fsItemSortedByNameTree.Insert( &this->dirNode );
    }
}

CVFS_TEMPLARGS
CVirtualFileSystem CVFS_TEMPLUSE::fsActiveEntry::~fsActiveEntry( void )
{
    CVirtualFileSystem *manager = this->manager;

    // Remove us from the AVL trees (if inside):
    if ( directory *parentDir = this->parentDir )
    {
        parentDir->fsItemSortedByNameTree.RemoveByNodeFast( &this->dirNode );
    }

    manager->fsItemSortedByOrderTree.RemoveByNodeFast( &this->orderByNode );
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif //_MSC_VER

#endif //_FILESYSTEM_VFS_IMPLEMENTATION_
