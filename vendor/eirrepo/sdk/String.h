/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/String.h
*  PURPOSE:     Optimized String implementation
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

// We can also use optimized memory semantics for Strings, aswell.
// After all appending to a String should not cast another malloc, just a resize of
// the underlying memory buffer. Why would STL rely on this weird double-the-backbuffer
// trick, anyway?

#ifndef _EIR_STRING_HEADER_
#define _EIR_STRING_HEADER_

#include "Exceptions.h"
#include "eirutils.h"
#include "MacroUtils.h"
#include "DataUtil.h"
#include "MetaHelpers.h"

#include "FixedString.h"

#include <type_traits>

namespace eir
{

template <eir::CharacterType charType, MemoryAllocator allocatorType, typename exceptMan = eir::DefaultExceptionManager>
struct String
{
    // TODO: think about copying the allocator aswell, when a copy-assignment is being done.

    // Make sure that templates are friends of each-other.
    template <eir::CharacterType, MemoryAllocator, typename> friend struct String;

    // When we talk about characters in this object we actually mean code-points that historically
    // were single characters but became redundant as single characters after the invention of
    // UTF-8 and other multi-codepoint encodings for internationalization.

private:
    AINLINE void reset_to_empty( void ) noexcept
    {
        this->data.char_data = (charType*)GetEmptyString <charType> ();
        this->data.num_chars = 0;
    }

public:
    inline String( void ) noexcept
    {
        this->reset_to_empty();
    }

private:
    AINLINE void initialize_with( const charType *initChars, size_t initCharCount )
    {
        if ( initCharCount == 0 )
        {
            this->reset_to_empty();
        }
        else
        {
            size_t copyCharSize = sizeof(charType) * ( initCharCount + 1 );

            charType *charBuf = (charType*)this->data.opt().Allocate( this, copyCharSize, alignof(charType) );

            if ( charBuf == nullptr )
            {
                exceptMan::throw_oom( copyCharSize );
            }

            //noexcept
            {
                // Copy the stuff.
                FSDataUtil::copy_impl( initChars, initChars + initCharCount, charBuf );

                // Put the zero terminator.
                *( charBuf + initCharCount ) = charType();

                // We take the data.
                this->data.char_data = charBuf;
                this->data.num_chars = initCharCount;
            }
        }
    }

    // Helper logic.
    static AINLINE void free_old_buffer( String *refMem, charType *oldChars, size_t oldCharCount, bool isNewBuf ) noexcept
    {
        if ( isNewBuf && oldCharCount > 0 )
        {
            refMem->data.opt().Free( refMem, oldChars );
        }
    }

public:
    template <typename... Args>
    inline String( const charType *initChars, size_t initCharCount, Args&&... allocArgs )
        : data( std::tuple(), std::tuple( std::forward <Args> ( allocArgs )... ) )
    {
        initialize_with( initChars, initCharCount );
    }

    template <typename... Args>
    inline String( constr_with_alloc, Args&&... allocArgs )
        : data( std::tuple(), std::tuple( std::forward <Args> ( allocArgs )... ) )
    {
        reset_to_empty();
    }

    inline String( const charType *initChars )
    {
        size_t initCharCount = cplen_tozero( initChars );

        initialize_with( initChars, initCharCount );
    }
    inline String( const FixedString <charType>& right )
    {
        size_t initCharCount = right.GetLength();

        initialize_with( right.GetConstString(), initCharCount );
    }

    inline String( const String& right ) : data( right.data )
    {
        // Simply create a copy.
        size_t copyCharCount = right.data.num_chars;
        const charType *src_data = right.data.char_data;

        initialize_with( src_data, copyCharCount );
    }

    template <typename... otherStringArgs>
    inline String( const String <charType, otherStringArgs...>& right )
    {
        // Simply create a copy.
        size_t copyCharCount = right.data.num_chars;
        const charType *src_data = right.data.char_data;

        initialize_with( src_data, copyCharCount );
    }

    template <MemoryAllocator otherAllocType, typename... otherStringArgs>
        requires ( eir::nothrow_constructible_from <allocatorType, otherAllocType&&> )
    inline String( String <charType, otherAllocType, otherStringArgs...>&& right ) noexcept : data( std::move( right.data.opt() ) )
    {
        this->data.char_data = right.data.char_data;
        this->data.num_chars = right.data.num_chars;

        right.reset_to_empty();
    }

private:
    AINLINE void release_data( void ) noexcept
    {
        free_old_buffer( this, this->data.char_data, this->data.num_chars, true );
    }

public:
    inline ~String( void )
    {
        this->release_data();
    }

private:
    // WARNING: we expect every operation after this buffer expansion to...
    // 1) not use the old buffer anymore
    // 2) to be noexcept
    AINLINE static void expand_buffer(
        String *refMem,
        charType *oldCharBuf, size_t oldCharCount, size_t oldCharCopyCount, size_t newCharCount,
        charType*& oldCharBufRef,
        charType*& useBufOut, bool& isBufNewOut
    )
    {
        // actually count of code-points, not characters.
        size_t newRequiredCPSize = sizeof(charType) * ( newCharCount + 1 );

        bool hasBuf = false;
        charType *useBuf = nullptr; // initializing this just to stop compiler warnings.
        bool isBufNew = false;

        if ( oldCharBuf && oldCharCount > 0 )
        {
            if ( newCharCount == 0 )
            {
                // Optimization.
                refMem->data.opt().Free( refMem, oldCharBuf );

                hasBuf = true;
                useBuf = nullptr;
                isBufNew = false;       // if we have no more buffer then we also have no new buffer, because we just free our current.
            }
            else
            {
                if constexpr ( ResizeMemoryAllocator <allocatorType> )
                {
                    bool couldResize = refMem->data.opt().Resize( refMem, oldCharBuf, newRequiredCPSize );

                    if ( couldResize )
                    {
                        hasBuf = true;
                        useBuf = oldCharBuf;
                        isBufNew = false;
                    }
                }
                else if constexpr ( ReallocMemoryAllocator <allocatorType> )
                {
                    charType *req_move_again = (charType*)refMem->data.opt().Realloc( refMem, oldCharBuf, newRequiredCPSize, alignof(charType) );

                    if ( req_move_again == nullptr )
                    {
                        exceptMan::throw_oom( newRequiredCPSize );
                    }

                    hasBuf = true;
                    useBuf = req_move_again;
                    oldCharBufRef = req_move_again;
                    isBufNew = false;
                }
            }
        }

        if ( hasBuf == false )
        {
            useBuf = (charType*)refMem->data.opt().Allocate( refMem, newRequiredCPSize, alignof(charType) );

            if ( useBuf == nullptr )
            {
                exceptMan::throw_oom( newRequiredCPSize );
            }

            // Guarranteed to throw no exception due to the trivial charType.
            size_t charCopyCount = std::min( oldCharCopyCount, newCharCount );

            if ( charCopyCount > 0 )
            {
                // Copy over the characters.
                FSDataUtil::copy_impl( oldCharBuf, oldCharBuf + charCopyCount, useBuf );
            }

            hasBuf = true;
            isBufNew = true;
        }

        // Return the data.
        isBufNewOut = isBufNew;
        useBufOut = useBuf;
    }

public:
    // *** Modification functions.

    inline void Assign( const charType *theChars, size_t copyCharCount )
    {
        if ( copyCharCount == 0 )
        {
            this->release_data();

            this->reset_to_empty();
        }
        else
        {
            // We need a big enough buffer for the new string.
            // If we have a buffer already then try scaling it.
            // Otherwise we allocate a new one.
            charType *useBuf = nullptr;
            bool isBufNew;

            charType *oldCharBuf = this->data.char_data;
            size_t oldCharCount = this->data.num_chars;

            expand_buffer( this, oldCharBuf, oldCharCount, 0, copyCharCount, this->data.char_data, useBuf, isBufNew );

            // Granted because charType is trivial.
            //noexcept
            {
                // Create a copy of the input strings.
                // We add 1 to include the null-terminator.
                FSDataUtil::copy_impl( theChars, theChars + copyCharCount + 1, useBuf );

                // Take over the buff.
                free_old_buffer( this, oldCharBuf, oldCharCount, isBufNew );

                this->data.char_data = useBuf;
                this->data.num_chars = copyCharCount;
            }
        }
    }

    inline String& operator = ( const String& right )
    {
        this->Assign( right.data.char_data, right.data.num_chars );

        return *this;
    }
    inline String& operator = ( const eir::FixedString <charType>& right )
    {
        this->Assign( right.GetConstString(), right.GetLength() );

        return *this;
    }

    template <typename... otherStringArgs>
    inline String& operator = ( const String <charType, otherStringArgs...>& right )
    {
        this->Assign( right.data.char_data, right.data.num_chars );

        return *this;
    }

    inline String& operator = ( const charType *rightChars )
    {
        this->Assign( rightChars, cplen_tozero( rightChars ) );

        return *this;
    }

    template <MemoryAllocator otherAllocType, typename... otherStringArgs>
        requires ( std::is_nothrow_assignable <allocatorType&, otherAllocType&&>::value )
    inline String& operator = ( String <charType, otherAllocType, otherStringArgs...>&& right ) noexcept
    {
        // Delete previous string.
        free_old_buffer( this, this->data.char_data, this->data.num_chars, true );

        // Move over allocator if needed.
        this->data.opt() = std::move( right.data.opt() );

        this->data.char_data = right.data.char_data;
        this->data.num_chars = right.data.num_chars;

        right.reset_to_empty();

        return *this;
    }

    // Append characters to the end of this string.
    inline void Append( const charType *charsToAppend, size_t charsToAppendCount )
    {
        // There is nothing to do.
        if ( charsToAppendCount == 0 )
            return;

        size_t num_chars = this->data.num_chars;

        // Calculate how long the new string has to be.
        size_t newCharCount = ( num_chars + charsToAppendCount );

        // Allocate the new buffer.
        charType *oldCharBuf = this->data.char_data;

        charType *useBuf;
        bool isBufNew;

        expand_buffer( this, oldCharBuf, num_chars, num_chars, newCharCount, this->data.char_data, useBuf, isBufNew );

        // Guarranteed due to trivial charType.
        //noexcept
        {
            // Now copy in the appended bytes.
            FSDataUtil::copy_impl( charsToAppend, charsToAppend + charsToAppendCount, useBuf + num_chars );

            // We must re-put the null-terminator.
            *( useBuf + newCharCount ) = (charType)0;

            // Take over the new buffer.
            free_old_buffer( this, oldCharBuf, num_chars, isBufNew );

            this->data.char_data = useBuf;
            this->data.num_chars = newCharCount;
        }
    }
    inline void Append( charType c )
    {
        Append( &c, 1 );
    }

    inline void Insert( size_t insertPos, const charType *charsToInsert, size_t charsToInsertCount )
    {
        // Nothing to do? Then just quit.
        if ( charsToInsertCount == 0 )
            return;

        // Expand the memory as required.
        size_t oldCharCount = this->data.num_chars;
        charType *oldCharBuf = this->data.char_data;

        // If the insertion position is after the string size, then we clamp the insertion
        // position to the size.
        if ( insertPos > oldCharCount )
        {
            insertPos = oldCharCount;
        }

        size_t newCharCount = ( oldCharCount + charsToInsertCount );

        charType *useBuf;
        bool isBufNew;

        expand_buffer( this, oldCharBuf, oldCharCount, insertPos, newCharCount, this->data.char_data, useBuf, isBufNew );

        //noexcept
        {
            // We first put any after-bytes to the end of the string.
            size_t afterBytesCount = ( oldCharCount - insertPos );

            if ( afterBytesCount > 0 )
            {
                const charType *move_start = ( oldCharBuf + insertPos );

                FSDataUtil::copy_backward_impl( move_start, move_start + afterBytesCount, useBuf + newCharCount );
            }

            // Now copy in the insertion chars.
            FSDataUtil::copy_impl( charsToInsert, charsToInsert + charsToInsertCount, useBuf + insertPos );

            // Put the zero terminator at the end.
            *( useBuf + newCharCount ) = (charType)0;

            // Take over the new stuff.
            free_old_buffer( this, oldCharBuf, oldCharCount, isBufNew );

            this->data.char_data = useBuf;
            this->data.num_chars = newCharCount;
        }
    }
    inline void Insert( size_t insertPos, charType c )
    {
        Insert( insertPos, &c, 1 );
    }

    // Delete a chunk of string.
    // Same method should be found at Vector.
    inline void RemovePart( size_t remPos, size_t remCount )
    {
        if ( remCount == 0 )
            return;

        charType *oldCharBuf = this->data.char_data;
        size_t oldCharCount = this->data.num_chars;

        if ( remPos >= oldCharCount )
            return;

        size_t rem_moveDownStartIdx = remPos + remCount;

        // We move down remCount characters starting from (remPos+remCount) to (remPos).
        // Then we just shrink the buffer.
        size_t newCharCount;

        if ( rem_moveDownStartIdx < oldCharCount )
        {
            size_t moveDownCount = std::min( oldCharCount - rem_moveDownStartIdx, oldCharCount );

            // We need to copy forward because otherwise we would overwrite stuff.
            FSDataUtil::copy_impl( oldCharBuf + rem_moveDownStartIdx, oldCharBuf + rem_moveDownStartIdx + moveDownCount, oldCharBuf + remPos );

            newCharCount = oldCharCount - remCount;
        }
        else
        {
            // Since we have nothing to move down we just trimmed-off the end.
            // Thus all that remains is before the removal offset.
            newCharCount = remPos;
        }

        // Now shrink away the invalid buffer end.
        charType *useBuf;
        bool isBufNew;

        expand_buffer( this, oldCharBuf, oldCharCount, newCharCount, newCharCount, this->data.char_data, useBuf, isBufNew );

        // Free away any old buf if we allocated new.
        free_old_buffer( this, oldCharBuf, oldCharCount, isBufNew );

        this->data.char_data = useBuf;
        this->data.num_chars = newCharCount;
    }

    // Turns the string around, so that it is written in reverse order.
    inline void reverse( void )
    {
        // We revert the beginning even-string.
        // Then if there was an uneven-item at the end, we insert it to the front.
        size_t cp_count = this->data.num_chars;

        if ( cp_count == 0 )
            return;

        bool is_uneven = ( ( cp_count & 0x01 ) == 1 );

        charType *data = this->data.char_data;

        // Revert the even part.
        {
            size_t even = ( is_uneven ? cp_count - 1 : cp_count );
            size_t half_of_even = ( cp_count / 2 );

            for ( size_t n = 0; n < half_of_even; n++ )
            {
                size_t left_idx = n;
                size_t right_idx = ( even - n - 1 );

                charType left_swap = std::move( data[ left_idx ] );
                charType right_swap = std::move( data[ right_idx ] );

                data[ left_idx ] = std::move( right_swap );
                data[ right_idx ] = std::move( left_swap );
            }
        }

        // If we are uneven, then we have to move up all the items by one now.
        if ( is_uneven )
        {
            size_t idx = ( cp_count - 1 );

            charType uneven_remember = data[ idx ];

            while ( idx > 0 )
            {
                size_t idx_to = idx;

                idx--;

                data[ idx_to ] = std::move( data[ idx ] );
            }

            data[ 0 ] = uneven_remember;
        }
    }

    // Empties out the string by freeing the associated buffer.
    inline void Clear( void ) noexcept
    {
        free_old_buffer( this, this->data.char_data, this->data.num_chars, true );

        this->reset_to_empty();
    }

    // Sets the amount of code points that this string is consisting of.
    // If the string grows then it is padded with zeroes.
    inline void Resize( size_t numCodePoints )
    {
        size_t oldCharCount = this->data.num_chars;

        // Nothing to do?
        if ( oldCharCount == numCodePoints )
            return;

        charType *oldCharBuf = this->data.char_data;

        charType *useBuf;
        bool isBufNew;

        expand_buffer( this, oldCharBuf, oldCharCount, numCodePoints, numCodePoints, this->data.char_data, useBuf, isBufNew );

        if ( numCodePoints == 0 )
        {
            reset_to_empty();
        }
        else
        {
            // Fill up the zeroes.
            for ( size_t idx = oldCharCount; idx < numCodePoints; idx++ )
            {
                useBuf[ idx ] = (charType)0;
            }

            // Zero-terminate.
            *( useBuf + numCodePoints ) = (charType)0;

            // Remember the new thing.
            this->data.char_data = useBuf;
            this->data.num_chars = numCodePoints;
        }

        // Destroy any previous buffer.
        free_old_buffer( this, oldCharBuf, oldCharCount, isBufNew );
    }

    // Returns true if the codepoints of compareWith match this string.
    // This check if of course case-sensitive.
    // Use other algorithms if you need an case-insensitive comparison because it is complicated (UniChar.h).
    inline bool equals( const charType *compareWith, size_t compareWithCount ) const
    {
        size_t num_chars = this->data.num_chars;

        if ( num_chars != compareWithCount )
            return false;

        // Do we find any codepoint that does not match?
        const charType *leftChars = this->data.char_data;

        for ( size_t n = 0; n < num_chars; n++ )
        {
            charType leftChar = *( leftChars + n );
            charType rightChar = *( compareWith + n );

            if ( leftChar != rightChar )
            {
                return false;
            }
        }

        return true;
    }

    inline bool equals( const charType *compareWith ) const
    {
        // TODO: maybe optimize this so that we loop only once.
        return equals( compareWith, cplen_tozero( compareWith ) );
    }

    inline bool IsEmpty( void ) const noexcept
    {
        return ( this->data.num_chars == 0 );
    }

    inline size_t GetLength( void ) const noexcept
    {
        return this->data.num_chars;
    }

    inline const charType* GetConstString( void ) const noexcept
    {
        return this->data.char_data;
    }

    inline FixedString <charType> ToFixed( void ) const noexcept
    {
        return FixedString( this->data.char_data, this->data.num_chars );
    }

    // Helpful operator overrides.

    inline String& operator += ( charType oneChar )
    {
        this->Append( &oneChar, 1 );

        return *this;
    }

    inline String& operator += ( const charType *someChars )
    {
        size_t appendCount = cplen_tozero( someChars );

        this->Append( someChars, appendCount );

        return *this;
    }

    template <typename... otherStringArgs>
    inline String& operator += ( const String <charType, otherStringArgs...>& right )
    {
        this->Append( right.data.char_data, right.data.num_chars );

        return *this;
    }

    inline String& operator += ( const FixedString <charType>& right )
    {
        this->Append( right.GetConstString(), right.GetLength() );

        return *this;
    }

    template <typename... otherStringArgs>
    inline String operator + ( const String <charType, otherStringArgs...>& right )
    {
        String newString( *this );

        newString += right;

        return newString;
    }

    friend inline bool operator == ( const String& left, const charType *someChars )
    {
        return left.equals( someChars, cplen_tozero( someChars ) );
    }
    friend inline bool operator == ( const charType *someChars, const String& right )
    {
        return right.equals( someChars, cplen_tozero( someChars ) );
    }

    friend inline bool operator == ( const String& left, const FixedString <charType>& right )
    {
        return left.equals( right.GetConstString(), right.GetLength() );
    }
    friend inline bool operator == ( const FixedString <charType>& left, const String& right )
    {
        return right.equals( left.GetConstString(), left.GetCount() );
    }

    template <typename... otherStringArgs>
    inline bool operator == ( const String <charType, otherStringArgs...>& right ) const
    {
        return this->equals( right.data.char_data, right.data.num_chars );
    }

    friend inline bool operator != ( const String& left, const charType *someChars )
    {
        return !( left == someChars );
    }
    friend inline bool operator != ( const charType *someChars, const String& right )
    {
        return !( someChars == right );
    }

    friend inline bool operator != ( const String& left, const FixedString <charType>& right )
    {
        return !( left == right );
    }
    friend inline bool operator != ( const FixedString <charType>& left, const String& right )
    {
        return !( left == right );
    }

    template <typename... otherStringArgs>
    inline bool operator != ( const String <charType, otherStringArgs...>& right ) const
    {
        return !( operator == ( right ) );
    }

    // Optimizations for the things.
    friend AINLINE String operator + ( const charType *left, String&& right )
    {
        right.Insert( 0, left, cplen_tozero( left ) );
        return std::move( right );
    }
    friend AINLINE String operator + ( String&& left, const charType *right )
    {
        left += right;
        return std::move( left );
    }
    friend AINLINE String operator + ( const charType *left, const String& right )
    {
        String outStr( eir::constr_with_alloc::DEFAULT, right.GetAllocData() );
        outStr += left;
        outStr += right;
        return outStr;
    }
    friend AINLINE String operator + ( const String& left, const charType *right )
    {
        String outStr( left );
        outStr += right;
        return outStr;
    }
    friend AINLINE String operator + ( charType left, String&& right )
    {
        right.Insert( 0, &left, 1 );
        return std::move( right );
    }
    friend AINLINE String operator + ( String&& left, charType right )
    {
        left += right;
        return std::move( left );
    }
    friend AINLINE String operator + ( charType left, const String& right )
    {
        String outStr( eir::constr_with_alloc::DEFAULT, right.GetAllocData() );
        outStr += left;
        outStr += right;
        return outStr;
    }
    friend AINLINE String operator + ( const String& left, charType right )
    {
        String outStr( left );
        outStr += right;
        return outStr;
    }

    // Reason why we do not provide less-than or greater-than operators is because it would only make
    // sense with case-insensitive functionality added.

    // Needed for operator overloads.
    inline auto GetAllocData( void ) const
    {
        return this->data.opt();
    }

private:
    // The actual members of the String object.
    // Only time will tell if they'll include static_if.
    // Maybe we will have a nice time with C++ concepts?
    // requires-clause for member variables could be nice aswell.
    struct fields
    {
        charType *char_data;
        size_t num_chars;
    };

    empty_opt <allocatorType, fields> data;
};

// Traits.
template <typename>
struct is_string : std::false_type
{};
template <typename... stringArgs>
struct is_string <String <stringArgs...>> : std::true_type
{};
template <typename... stringArgs>
struct is_string <const String <stringArgs...>> : std::true_type
{};
template <typename... stringArgs>
struct is_string <String <stringArgs...>&> : std::true_type
{};
template <typename... stringArgs>
struct is_string <String <stringArgs...>&&> : std::true_type
{};
template <typename... stringArgs>
struct is_string <const String <stringArgs...>&> : std::true_type
{};
template <typename... stringArgs>
struct is_string <const String <stringArgs...>&&> : std::true_type
{};

} // namespace eir

#endif //_EIR_STRING_HEADER_
