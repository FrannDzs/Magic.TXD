#ifndef _SYNTAX_
#define _SYNTAX_

#include <stddef.h>

#include <sdk/String.h> // for token to double/int.
#include <sdk/UniChar.h>

#include <string>
#include <sdk/NumericFormat.h>

struct syntax_exception {};
struct bad_format_exception {};

namespace SyntaxHelpers
{
    enum class eTokenType
    {
        NAME,
        NUMERIC,
        SINGLE
    };

    enum class eCharacterClass
    {
        NAME,
        NUMERIC,
        WHITESPACE,
        RENDERABLE
    };
};

template <typename encodingCharType>
class CSyntax
{
public:
    typedef SyntaxHelpers::eTokenType eTokenType;
    typedef character_env <encodingCharType> char_env;
    typedef typename char_env::ucp_t ucp_t;

    typedef charenv_charprov_tocplen <encodingCharType> char_prov;
    typedef character_env_iterator <encodingCharType, char_prov> char_iter;

    CSyntax(const encodingCharType *buffer, size_t size, size_t offset = 0)
    {
        m_buffer = buffer;

        m_offset = 0;
        m_size = size;
    }
    CSyntax(void) : CSyntax( nullptr, 0 )
    {
        return;
    }
    CSyntax(const CSyntax& right) = default;
    CSyntax(CSyntax&& right)
    {
        this->m_buffer = right.m_buffer;
        this->m_offset = right.m_offset;
        this->m_size = right.m_size;

        right.m_buffer = nullptr;
        right.m_offset = 0;
        right.m_size = 0;
    }
    ~CSyntax(void)
    {
    }

    CSyntax&            operator = (const CSyntax& right) = default;
    CSyntax&            operator = (CSyntax&& right)
    {
        this->m_buffer = right.m_buffer;
        this->m_offset = right.m_offset;
        this->m_size = right.m_size;

        right.m_buffer = nullptr;
        right.m_offset = 0;
        right.m_size = 0;

        return *this;
    }

private:
    template <typename charType>
    static inline bool IsName(charType c)
    {
	    if ( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == (unsigned char)'ß' || c == (unsigned char)'_' || c > 256 ||
             c == (unsigned char)'ä' || c == (unsigned char)'Ä' ||
             c == (unsigned char)'ö' || c == (unsigned char)'Ö' ||
             c == (unsigned char)'ü' || c == (unsigned char)'Ü' )
        {
		    return true;
	    }

        return false;
    }

    template <typename charType>
    static inline bool IsNumeric(charType c)
    {
        switch( c )
        {
        case (charType)'1':
        case (charType)'2':
        case (charType)'3':
        case (charType)'4':
        case (charType)'5':
        case (charType)'6':
        case (charType)'7':
        case (charType)'8':
        case (charType)'9':
        case (charType)'0':
            return true;
        }

        return false;
    }

    template <typename charType>
    static inline bool IsSpace(charType c)
    {
        switch( c )
        {
        case (charType)' ': case (charType)'\t':
            return true;
        }

        if ( c == (unsigned char)0xA0 ) // check for non-breaking space.
        {
            return true;
        }

        return false;
    }

    template <typename charType>
    static inline bool IsNewline(charType c)
    {
        switch( c )
        {
        case (charType)'\n':
        case (charType)'\r':
            return true;
        }

        return false;
    }

    template <typename charType>
    static inline bool is_char_renderable( charType c )
    {
        // Have to verify where this still makes sense in the unicode context.
        return ( c >= 0x11 );
    }

    template <typename charType>
    static inline bool is_class_char( const charType *tokenChars, ucp_t c, bool caseSensitive )
    {
        toupper_lookup <charType> facet_tokChars( std::locale::classic() );
        toupper_lookup <ucp_t> facet_ourChars( std::locale::classic() );

        character_env_iterator_tozero <charType> iter( tokenChars );

        while ( !iter.IsEnd() )
        {
            auto tok_c = iter.ResolveAndIncrement();

            if ( IsCharacterEqualEx( tok_c, c, facet_tokChars, facet_ourChars, caseSensitive ) )
            {
                return true;
            }

            tokenChars++;
        }

        return false;
    }

    template <typename multiCharType, typename singleCharType>
    static inline bool IsTokenFoundSame(const multiCharType *tokens, const singleCharType *tok, size_t tokLen, eTokenType tokType, bool caseSensitive)
    {
        if ( UniversalCompareStrings( tokens, tokLen, tok, tokLen, caseSensitive ) == false )
        {
            return false;
        }

        tokens += tokLen;

        multiCharType termChar = *tokens;

        // Must make sure that the token in the input string has finished.
        if ( tokType == CSyntax::eTokenType::NAME )
        {
            if ( IsName( termChar ) )
            {
                return false;
            }
        }
        else if ( tokType == CSyntax::eTokenType::NUMERIC )
        {
            if ( IsName( termChar ) || IsNumeric( termChar ) || termChar == (multiCharType)'.' || termChar == (multiCharType)'-' )
            {
                return false;
            }
        }
        else if ( tokType == CSyntax::eTokenType::SINGLE )
        {
            // Single tokens always do good.
            return true;
        }
        else
        {
            // ALSO FOR SINGLE TOKEN.

            // Approximate using space or end.
            if ( termChar != (multiCharType)0 && !IsSpace( termChar ) )
            {
                return false;
            }
        }

        return true;
    }

public:
    bool Finished() const
    {
        return ( m_offset == m_size );
    }

    ucp_t ReadNext(size_t& cpLenOut)
    {
        if ( Finished() )
            return 0;

        const encodingCharType *cur_ptr = (m_buffer + m_offset);
        size_t left_size = ( this->m_size - this->m_offset );

        character_env_iterator <encodingCharType, char_prov> iter( cur_ptr, char_prov( cur_ptr, left_size ) );

        size_t advLen = iter.GetIterateCount();

        ucp_t retChar = iter.Resolve();

        this->m_offset += advLen;

        cpLenOut = advLen;

        return retChar;
    }

    ucp_t ReadNextSimple()
    {
        size_t cpReadOut;

        return ReadNext( cpReadOut );
    }

private:
    bool ScanToVisibleChars(bool parseDirect, ucp_t& outChar, size_t& cpLenOut)
    {
        size_t read_cpLen;
        ucp_t read;

        do
        {
            if ( Finished() )
                return false;

            read = ReadNext( read_cpLen );

            if ( is_char_renderable( read ) && ( parseDirect || !IsSpace( read ) ) )
            {
                break;
            }

        } while ( true );

        outChar = read;
        cpLenOut = read_cpLen;
        return true;
    }

public:
    void                SkipNonVisible(bool parseDirect = false)
    {
        size_t last_read_cpLen;
        ucp_t last_read;

        bool foundSomething = this->ScanToVisibleChars( parseDirect, last_read, last_read_cpLen );

        if ( foundSomething )
        {
            DecreaseSeek( last_read_cpLen );
        }
    }

    const encodingCharType*     ParseTokenEx(size_t *len, eTokenType& tokenType, bool parseDirect = false)
    {
        // parseDirect means that we do not skip whitespace before a token.

        size_t read_cpLen;
        ucp_t read;

        if ( !ScanToVisibleChars( parseDirect, read, read_cpLen ) )
        {
            return nullptr;
        }

        // If we are on space or newline chars then we are nothing.
        if ( IsSpace( read ) || IsNewline( read ) )
        {
            return nullptr;
        }

        // Check if it a pure name.
        if ( IsName(read) )
        {
            DecreaseSeek( read_cpLen );

            tokenType = eTokenType::NAME;
            return ParseName(len);
        }

        // Check if it could be a numeric.
        if ( IsNumeric(read) || read == '-' || read == '.' )
        {
            DecreaseSeek( read_cpLen );

            size_t seekWant = this->GetOffset();

            if ( const encodingCharType *numStr = ParseNumber( len ) )
            {
                tokenType = eTokenType::NUMERIC;
                return numStr;
            }

            this->SetOffset( seekWant + 1 );
        }

        // Check if it starts like a numeric but really is a name.
        if ( IsNumeric(read) )
        {
            DecreaseSeek( read_cpLen );

            if ( const encodingCharType *nameStr = this->ParseName(len) )
            {
                tokenType = eTokenType::NAME;
                return nameStr;
            }
        }

        tokenType = eTokenType::SINGLE;
        *len = read_cpLen;
        return ( m_buffer + m_offset - read_cpLen );
    }
    const encodingCharType*     ParseToken(size_t *lenOut, bool parseDirect = false)
    {
        eTokenType tokType;

        return this->ParseTokenEx( lenOut, tokType, parseDirect );
    }
    const encodingCharType*	    ParseName(size_t *len)
    {
        size_t offset = GetOffset();
        const encodingCharType *name = ( m_buffer + offset );

        bool hasFinished = false;
        size_t lastChar_cpLen;

        while ( true )
        {
            if ( Finished() )
            {
                hasFinished = true;
                break;
            }

            ucp_t read = ReadNext( lastChar_cpLen );

            if ( !( IsName( read ) || IsNumeric( read ) ) )
            {
                break;
            }
        }

        if ( !hasFinished )
        {
            DecreaseSeek( lastChar_cpLen );
        }

        *len = GetOffset() - offset;
        return name;
    }
    const encodingCharType*     ParseCustomToken(const char *tokenChars, size_t& tokenLen, bool caseSensitive = true, bool parseDirect = false)
    {
        size_t read_cpLen;
        ucp_t read;

        if ( !ScanToVisibleChars( parseDirect, read, read_cpLen ) )
        {
            return nullptr;
        }

        // If we are on space or newline chars then we are nothing.
        if ( IsSpace( read ) || IsNewline( read ) )
        {
            return nullptr;
        }

        // We parse an entire token.
        // It is defined by a number of characters that come after each other.
        // For those kind of people that expect certain tokens and do not like the already provided types.
        size_t startOff = ( this->GetOffset() - 1 );

        const encodingCharType *start_tok = ( this->m_buffer + startOff );

        bool hasFinished = false;
        bool hasVerifiedToken = false;

        while ( true )
        {
            if ( tokenChars != nullptr )
            {
                if ( is_class_char( tokenChars, read, caseSensitive ) == false )
                {
                    break;
                }
            }
            else
            {
                // Try any until newline.
                if ( IsNewline( read ) || IsSpace( read ) )
                {
                    break;
                }
            }

            hasVerifiedToken = true;

            if ( Finished() )
            {
                hasFinished = true;
                break;
            }

            read = this->ReadNext( read_cpLen );
        }

        if ( !hasVerifiedToken )
        {
            return nullptr;
        }

        if ( !hasFinished )
        {
            // Subtract the char that we failed to read.
            DecreaseSeek( read_cpLen );
        }

        tokenLen = ( this->GetOffset() - startOff );
        return start_tok;
    }
    bool                IsCurrentNumber(void)
    {
        size_t curOff = this->m_offset;

        size_t numLen;
        const encodingCharType *numStr = this->ParseNumber( &numLen );

        this->m_offset = curOff;

        return ( numStr != nullptr );
    }
    const encodingCharType*     ParseNumber(size_t *len)
    {
        size_t offset = GetOffset();
        const encodingCharType *numStr = m_buffer + offset;

        // If we have a negative mark, we skip it and all the whitespace.
        // It is only a number if we find the numeric part anyway.
        size_t c_cpLen;
        ucp_t c = ReadNext( c_cpLen );

        bool didHaveNumeric = false;

        if ( c == '-' )
        {
            // Skip any possible whitespace.
            this->SkipWhitespace();

            c = ReadNext(c_cpLen );
        }

        // Determine the length of the possible number.
        while ( IsNumeric( c ) )
        {
            c = ReadNext( c_cpLen );

            didHaveNumeric = true;
        }

        // If we have a dot, we could be a floating point.
        if ( c == '.' )
        {
            c = ReadNext( c_cpLen );

            // Parse over all the numbers.
            while ( IsNumeric( c ) )
            {
                c = ReadNext( c_cpLen );

                didHaveNumeric = true;
            }
        }

        // If we found no numeric at all, we ain't a number.
        if ( !didHaveNumeric )
            return nullptr;

        // Check that the thing is really a number.
        if ( !Finished() )
        {
            // If we did not finish then we could have ended in a not-a-number constant being very bad.
            // Check that this is not the case.
            if ( IsName( c ) )
            {
                return nullptr;
            }

            // Seek to prior before the end test.
            DecreaseSeek( c_cpLen );
        }

        *len = GetOffset() - offset;
        return numStr;
    }

    const encodingCharType*     ParseTokenOfType(size_t *len, eTokenType expectType, bool parseDirect = false)
    {
        //size_t reset_off = this->GetOffset();

        eTokenType tokType;
        size_t tokLen;
        const encodingCharType *tok = this->ParseTokenEx( &tokLen, tokType, parseDirect );

        if ( !tok )
            return nullptr;

        if ( tokType != expectType )
        {
            // If we failed due to type then we must NOT pollute the parser by trashing the seek.
            this->DecreaseSeek( tokLen );
            return nullptr;
        }

        *len = tokLen;
        return tok;
    }

    static int          TokenToInt(const encodingCharType *tok, size_t tokLen)
    {
        eir::String <char, CRTHeapAllocator> tmpStr = CharacterUtil::ConvertStringsLength <encodingCharType, char, CRTHeapAllocator> ( tok, tokLen );

        try
        {
            return eir::to_number_len( tok, tokLen );
        }
        catch( eir::codepoint_exception& )
        {
            throw bad_format_exception();
        }
    }
    static unsigned int TokenToUInt(const encodingCharType *tok, size_t tokLen);
    static double       TokenToDouble(const encodingCharType *tok, size_t tokLen)
    {
        //TODO: properly implement this, because pipelining through STL is fucking ugly.

        eir::String <char, CRTHeapAllocator> tmpStr = CharacterUtil::ConvertStringsLength <encodingCharType, char, CRTHeapAllocator> ( tok, tokLen );

        std::basic_string <char> numStr( tmpStr.GetConstString(), tmpStr.GetLength() );

        try
        {
            return std::stod( numStr );
        }
        catch( std::invalid_argument& )
        {
            throw bad_format_exception();
        }
    }

    template <typename checkCharType>
    bool                HasToken(const checkCharType *tokenStr, bool direct = false, bool caseSensitive = false, bool advanceAnyway = false)
    {
        size_t begOff = 0;

        if ( !advanceAnyway )
        {
            begOff = this->GetOffset();
        }

        size_t tokenLen;
        const encodingCharType *token = this->ParseToken( &tokenLen, direct );

        if ( !token || !BoundedStringEqual( token, tokenLen, tokenStr, caseSensitive ) )
        {
            if ( !advanceAnyway )
            {
                this->SetOffset( begOff );
            }

            return false;
        }

        return true;
    }
    bool                HasTokenOfType(eTokenType findTokType, bool direct = false)
    {
        size_t begOff = this->GetOffset();

        eTokenType actualTokType;
        size_t actualTokLen;
        const encodingCharType *actualTok = ParseTokenEx( &actualTokLen, actualTokType, direct );

        if ( actualTok == nullptr || actualTokLen == 0 || actualTokType != findTokType )
        {
            this->SetOffset( begOff );

            return false;
        }

        return true;
    }
    bool                HasTokenSequence(const char *tokens, bool isDirect = false, bool caseSensitive = true, bool concat = false)
    {
        size_t begOff = this->GetOffset();

        bool hadPrevToken = ( concat == true );

        while ( *tokens != '\0' )
        {
            eTokenType tokType;
            size_t tokLen;
            const encodingCharType *tok = ParseTokenEx( &tokLen, tokType, hadPrevToken && isDirect );

            if ( !tok )
                goto hasNothingOfValue;

            // Check if the tokens are equal.
            if ( !IsTokenFoundSame( tokens, tok, tokLen, tokType, caseSensitive ) )
            {
                goto hasNothingOfValue;
            }

            tokens += tokLen;

            hadPrevToken = true;

            // Skip any whitespace.
            while ( IsSpace( *tokens ) )
            {
                tokens++;
            }
        }

        return true;

    hasNothingOfValue:
        this->SetOffset( begOff );

        return false;
    }
    bool                HasAnyToken(bool direct = false)
    {
        size_t startOff = this->GetOffset();

        size_t tokLen;
        const encodingCharType *tok = ParseToken( &tokLen, direct );

        if ( tok == nullptr || tokLen == 0 )
        {
            this->SetOffset( startOff );

            return false;
        }

        return true;
    }
    template <typename checkCharType>
    bool                HasCharacter(checkCharType cp, bool case_sensitive = true)
    {
        if ( Finished() )
        {
            return false;
        }

        size_t our_ucp_len;

        auto our_ucp = this->ReadNext( our_ucp_len );

        if ( IsCharacterEqual( cp, our_ucp, case_sensitive ) )
        {
            return true;
        }

        this->DecreaseSeek( our_ucp_len );

        return false;
    }
    template <typename checkCharType>
    void                ExpectToken(const checkCharType *tokenStr)
    {
        size_t tokenLen;
        const encodingCharType *token = this->ParseToken( &tokenLen );

        if ( !token )
            throw syntax_exception();

        if ( !BoundedStringEqual( token, tokenLen, tokenStr, false ) )
        {
            throw syntax_exception();
        }
    }

    const encodingCharType*         ReadUntilNewLine(size_t *len)
    {
        const encodingCharType *curOff = ( m_buffer + this->GetOffset() );
        size_t theLength = 0;

        while ( !this->Finished() )
        {
            ucp_t outchar = this->ReadNextSimple();

            if ( outchar == '\r' || outchar == '\n' )
                break;

            theLength++;
        }

        if ( len )
        {
            *len = theLength;
        }
        return curOff;
    }

    bool				GotoNewLine()
    {
        bool skipped = SkipWhitespace();

        if ( !skipped )
        {
            return false;
        }

        ucp_t read = this->ReadNextSimple();

        if ( read == '\r' )
        {
            read = ReadNextSimple();
        }

        return ( read == '\n' );
    }
    bool                TryGotoNewLine()
    {
        size_t startOff = this->GetOffset();

        if ( !GotoNewLine() )
        {
            this->SetOffset( startOff );

            return false;
        }

        return true;
    }
    void                SkipLine()
    {
        while ( !Finished() )
        {
            ucp_t c = ReadNextSimple();

            if ( c == '\n' )
            {
                return;
            }
        }
    }
    bool    SkipWhitespace()
    {
        if ( this->Finished() )
        {
            return false;
        }

        bool hasFinished = false;
        size_t read_cpLen;

        while ( true )
        {
            if ( this->Finished() )
            {
                hasFinished = true;
                break;
            }

            ucp_t c = ReadNext( read_cpLen );

            if ( IsSpace( c ) == false )
            {
                break;
            }
        }

        if ( !hasFinished )
        {
            DecreaseSeek( read_cpLen );
        }

        return true;
    }
    bool				ScanCharacter(char c)
    {
        while ( !Finished() )
        {
            if ( ReadNextSimple() == c )
            {
                return true;
            }
        }

        return false;
    }
    const encodingCharType* ScanCharacterObvious(char c, bool *did_find = nullptr)
    {
        const encodingCharType *cur_res = ( this->m_buffer + this->m_offset );

        bool has_actually_found = false;

        while ( !Finished() )
        {
            if ( ReadNextSimple() == c )
            {
                has_actually_found = true;

                break;
            }

            cur_res = ( this->m_buffer + this->m_offset );
        }

        if ( did_find )
        {
            *did_find = has_actually_found;
        }

        return cur_res;
    }
    bool				ScanCharacterEx(char c, bool ignoreName, bool ignoreNumber, bool ignoreNewline)
    {
        size_t real_off = this->m_offset;

        size_t leftSize = ( this->m_size - real_off );
        const encodingCharType *start_buf = ( this->m_buffer + real_off );

        char_iter iter( start_buf, char_prov( start_buf, leftSize ) );

        while ( !iter.IsEnd() )
        {
            ucp_t read = iter.Resolve();

            if ( read == c )
            {
                this->m_offset += iter.GetIterateCount();

                return true;
            }

            if ( !ignoreName && IsName( read ) )
                return false;

            if ( !ignoreNewline && IsNewline( read ) )
                return false;

            if ( !ignoreNumber && IsCurrentNumber() )
                return false;

            this->m_offset += iter.GetIterateCount();

            iter.Increment();
        }

        return false;
    }
    size_t              FindCharacter(char c)
    {
        while ( !Finished() )
        {
            size_t readCPLen;
            ucp_t readChar = ReadNext( readCPLen );

            if ( readChar == c )
            {
                return ( GetOffset() - readCPLen );
            }
        }

        return GetOffset();
    }
    template <typename checkCharType>
    bool                ScanToken(const checkCharType *token, bool caseSensitive = true)
    {
        while ( !this->Finished() )
        {
            size_t tokLen;
            const encodingCharType *tok = this->ParseToken( &tokLen );

            if ( !tok )
                return false;

            if ( BoundedStringEqual( tok, tokLen, token, caseSensitive ) )
            {
                return true;
            }
        }

        return false;
    }
    template <typename checkCharType>
    bool                ScanTokenSequence(const checkCharType *tokens, bool isDirect, bool caseSensitive)
    {
        while ( !Finished() )
        {
            // Check if the first token is correct.
            CSyntax::eTokenType tokType;
            size_t tokLen;
            const encodingCharType *tok = ParseTokenEx( &tokLen, tokType );

            if ( !tok )
                throw syntax_exception();

            if ( IsTokenFoundSame( tokens, tok, tokLen, tokType, caseSensitive ) )
            {
                size_t tok_jumped_off = this->GetOffset();

                const checkCharType *next_toks = tokens + tokLen;

                // The first token is the same.
                while ( IsSpace( *next_toks ) )
                {
                    next_toks++;
                }

                // We are ok if we found the remainder.
                if ( HasTokenSequence( next_toks, isDirect, caseSensitive, true ) )
                {
                    return true;
                }

                this->SetOffset( tok_jumped_off );
            }
        }

        return false;
    }
    template <typename checkCharType>
    bool                ScanTokenAny(const checkCharType *tokens, size_t& findOff, bool caseSensitive = true)
    {
        size_t tokens_len = cplen_tozero( tokens );

        while ( !Finished() )
        {
            size_t tokPos = this->GetOffset();

            size_t tokLen;
            eTokenType tokType;
            const encodingCharType *tok = this->ParseTokenEx( &tokLen, tokType );

            if ( !tok )
                break;

            // Do process for tokens.
            CSyntax <checkCharType> findTokParser( tokens, tokens_len );

            while ( !findTokParser.Finished() )
            {
                size_t anyTokLen;
                const checkCharType *anyTok = findTokParser.ParseTokenOfType( &anyTokLen, tokType );

                if ( !anyTok )
                    break;

                if ( UniversalCompareStrings( tok, tokLen, anyTok, anyTokLen, caseSensitive ) )
                {
                    // OK.
                    findOff = tokPos;
                    return true;
                }
            }
        }

        return false;
    }
	template <typename checkCharType>
	bool                ScanStringLen( const checkCharType *string, size_t string_len, size_t& findOff, bool caseSensitive = true )
	{
		if ( string_len == 0 )
			return false;

		size_t ourSize = this->m_size;
		size_t ourSeek = this->m_offset;
		const encodingCharType *ourData = this->m_buffer;

		size_t left_cnt = ( ourSize - ourSeek );

		if ( left_cnt < string_len )
        {
            this->m_offset = ourSize;

			return false;
        }

		character_env_iterator <encodingCharType, charenv_charprov_tocplen <encodingCharType>> our_iter(
			ourData + ourSeek,
			charenv_charprov_tocplen <encodingCharType> ( ourData + ourSeek, left_cnt - string_len + 1 )
		);

		while ( true )
		{
			if ( our_iter.IsEnd() )
			{
				break;
			}

			const encodingCharType *curDataPtr = ( our_iter.GetPointer() );

			// See if we find the character sequence.
			if ( UniversalCompareStrings( string, string_len, curDataPtr, string_len, caseSensitive ) == true )
			{
				findOff = ( curDataPtr - ourData );

				this->m_offset = ( our_iter.GetPointer() - ourData ) + string_len;

				return true;
			}

			// Try next position.
			our_iter.Increment();
		}

		this->m_offset = ourSize;

		return false;
	}
	template <typename checkCharType>
	bool                ScanString( const checkCharType *string, size_t& findOff, bool caseSensitive = true )
    {
        return ScanStringLen( string, cplen_tozero( string ), findOff, caseSensitive );
    }
    template <typename checkCharType>
    const encodingCharType* ScanStringObviousLen( const checkCharType *string, size_t strLen, bool caseSensitive = true, bool *didFindOut = nullptr )
    {
        size_t findOff;

        if ( !ScanStringLen( string, strLen, findOff, caseSensitive ) )
        {
            if ( didFindOut != nullptr )
            {
                *didFindOut = false;
            }

            return this->GetEndOfData();
        }

        if ( didFindOut != nullptr )
        {
            *didFindOut = true;
        }

        return ( this->m_buffer + findOff );
    }
    template <typename checkCharType>
    const encodingCharType* ScanStringObvious( const checkCharType *string, bool caseSensitive = true, bool *didFindOut = nullptr )
    {
        return ScanStringObviousLen( string, cplen_tozero( string ), caseSensitive, didFindOut );
    }

    // Character class functions.
    void ScanWhileCharacterClass( SyntaxHelpers::eCharacterClass charType )
    {
        while ( !Finished() )
        {
            size_t ucp_len;

            auto ucp = this->ReadNext( ucp_len );

            if ( ( charType == SyntaxHelpers::eCharacterClass::NUMERIC && !IsNumeric( ucp ) ) ||
                 ( charType == SyntaxHelpers::eCharacterClass::NAME && !IsName( ucp ) ) ||
                 ( charType == SyntaxHelpers::eCharacterClass::WHITESPACE && !IsSpace( ucp ) ) ||
                 ( charType == SyntaxHelpers::eCharacterClass::RENDERABLE && !is_char_renderable( ucp ) ) )
            {
                this->DecreaseSeek( ucp_len );

                break;
            }
        }
    }

    const encodingCharType* GetCurrentData( void ) const
    {
        return ( m_buffer + m_offset );
    }
    size_t              GetCurrentDataSize( void ) const
    {
        return ( m_size - m_offset );
    }
    const encodingCharType* GetEndOfData( void ) const
    {
        return ( m_buffer + m_size );
    }
    size_t		        GetOffset()
    {
        return m_offset;
    }
    void				SetOffset(size_t offset)
    {
        m_offset = offset;
    }

    void                DecreaseSeek(size_t seek)
    {
        m_offset -= seek;
    }
    void				Seek(long seek)
    {
        m_offset = std::max(std::min((size_t)( m_offset + seek ), m_size), (size_t)0);
    }

private:
    const encodingCharType*     m_buffer;

    size_t              m_offset;
    size_t              m_size;
};

#endif //_SYNTAX_
