/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/UniChar.h
*  PURPOSE:     Character environment to parse characters in strings properly
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIRREPO_CHARACTER_RESOLUTION_
#define _EIRREPO_CHARACTER_RESOLUTION_

#include <cstdint>
#include "Exceptions.h"
#include "Endian.h"
#include "String.h"
#include "CharFundamentals.h"

#include <locale>
#include <type_traits>

#include <limits>

#include "eirutils.h"

// UNICODE CHARACTER LIBRARY TESTING.
template <typename charType>
struct character_env
{
    //static_assert( false, "unknown character encoding" );
};

// The idea of character_env is to support transcoding of characters between different stream encodings.
// Character providers for character_env are holders of string pointers that allow fetching of code points.
// In case of fetching a codepoint, we deliver a failure-of-fetch only if the fetch target is exactly at
// the end of stream. This is designed to be a burden only on the implementors of character_env, but considering
// that we must not skip reading any character and we always read by units of codepoint this aint too bad.

// Character provider that ends at zero-byte in stream.
template <typename charType>
struct charenv_charprov_tozero
{
	const charType *iter;
    mutable bool has_ended;

	AINLINE charenv_charprov_tozero( const charType *str )
	{
		this->iter = str;
        this->has_ended = false;
	}

private:
	static AINLINE bool is_zero( charType cp )
	{
		return ( cp == (charType)0 );
	}

public:
	AINLINE bool IsEndAt( size_t off ) const
	{
		return is_zero( *( iter + off ) );
	}

	AINLINE bool FetchUnit( size_t off, charType& unitOut ) const
	{
        if ( this->has_ended )
        {
            return false;
        }

		charType codepoint = *( iter + off );

		if ( is_zero( codepoint ) )
		{
			this->has_ended = true;
		}

		// TODO: certain streams can be encoded in a certain endianness.
		//  We need to take that into account someday

		unitOut = codepoint;
		return true;
	}
};

template <typename charType>
struct charenv_charprov_tocplen
{
	const charType *iter;
	size_t length;

	AINLINE charenv_charprov_tocplen( const charType *iter, size_t length )
	{
		this->iter = iter;
		this->length = length;
	}

	AINLINE bool IsEndAt( size_t off ) const
	{
		return ( off == this->length );
	}

	AINLINE bool FetchUnit( size_t off, charType& unitOut ) const
	{
		if ( off == this->length )
		{
			return false;
		}

		// TODO: certain streams can be encoded in a certain endianness.
		//  We need to take that into account someday.

		unitOut = *( this->iter + off );
		return true;
	}
};

template <typename charType, typename char_prov_t>
struct character_env_iterator
{
	typedef character_env <charType> char_env;

	typedef typename char_env::ucp_t ucp_t;

	char_prov_t char_prov;
	const charType *str;
    size_t n;

    AINLINE character_env_iterator( const charType *str, char_prov_t&& char_prov )
		: char_prov( std::move( char_prov ) )
    {
		this->str = str;
		this->n = 0;
    }

    AINLINE bool IsEnd( void ) const
	{
        return ( this->char_prov.IsEndAt( this->n ) );
    }

    AINLINE size_t GetIterateCount( void ) const
    {
        return ( char_env::GetStreamCodepointSize( this->n, this->char_prov ) );
    }

    AINLINE void Increment( void )
    {
        this->n += this->GetIterateCount();
    }

    AINLINE ucp_t Resolve( void ) const
    {
		size_t iter = this->n;

		size_t cpsize = char_env::GetStreamCodepointSize( iter, this->char_prov );

        return char_env::ResolveStreamCodepoint( cpsize, this->str + iter );
    }

    AINLINE ucp_t ResolveAndIncrement( void )
    {
		size_t iter = this->n;

		size_t cpsize = char_env::GetStreamCodepointSize( iter, this->char_prov );

        ucp_t res = char_env::ResolveStreamCodepoint( cpsize, this->str + iter );

		this->n = ( iter + cpsize );

        return res;
    }

    AINLINE const charType* GetPointer( void ) const
    {
        return ( this->str + this->n );
    }
};

// Special case that is used very often.
template <typename charType>
struct character_env_iterator_tozero : public character_env_iterator <charType, charenv_charprov_tozero <charType>>
{
    AINLINE character_env_iterator_tozero( const charType *str ) : character_env_iterator <charType, charenv_charprov_tozero <charType>> ( str, charenv_charprov_tozero <charType> ( str ) )
    {
        return;
    }
};

// ANSI string environment.
template <>
struct character_env <char>
{
    typedef char ucp_t; // UNICODE CODE POINT, can represent all characters.

    // ANSI strings are very simple and fast. Each byte represents one unique character.
    // This means that the number of bytes equals the string length.
	template <typename char_prov>
	static AINLINE size_t GetStreamCodepointSize( size_t off, const char_prov& prov )
	{
		return 1;
	}

	static AINLINE ucp_t ResolveStreamCodepoint( size_t cpsize, const char *iter )
	{
		// cpsize is always one.
		return *iter;
	}

	static constexpr char cp_unknown = '_';
	static constexpr ucp_t ucp_unknown = '_';
    static constexpr size_t ucp_max = std::numeric_limits <std::uint8_t>::max();

    struct enc_result
    {
        ucp_t data[1];
        size_t numData;
    };

    static AINLINE void EncodeUCP( ucp_t codepoint, enc_result& resOut )
    {
        resOut.data[0] = codepoint;
        resOut.numData = 1;
    }
};

// UTF-16 encoding.
template <typename wideCharType, wideCharType cp_unknown_param>
struct utf16_character_env
{
    typedef char32_t ucp_t; // UNICODE CODE POINT, can represent all characters.

private:
    struct code_point_hi_surrogate
    {
        std::uint16_t hiorder_cp : 6;
        std::uint16_t plane_id : 4;
        std::uint16_t checksum : 6; // 54 in decimals
    };

    struct code_point_lo_surrogate
    {
        std::uint16_t loworder_cp : 10;
        std::uint16_t checksum : 6; // 55 in decimals
    };

    struct result_code_point
    {
        union
        {
            struct
            {
                ucp_t lo_order : 10;
                ucp_t hi_order : 6;
                ucp_t plane_id : 5;
                ucp_t pad : 11; // ZERO OUT.
            };
            ucp_t value;
        };
    };

    struct code_point
    {
        union
        {
            std::uint16_t ucp;
            code_point_hi_surrogate hi_surrogate;
        };
    };

public:
    // Parsing UTF-16 character points is actually pretty complicated.
    // We support all characters of the world in 16bit code points, but many
    // characters require two 16bit code points because they are outside 0-0xFFFF (BMP).

    // Now comes the catch: detect how many code points we require to the next
    // code point! Thankfully UTF-16 will _never_ change.... rite?
	template <typename char_prov>
    static AINLINE size_t GetStreamCodepointSize( size_t off, const char_prov& prov )
    {
        // TODO: take into account endianness.

		union
		{
			wideCharType first;
			code_point cp;
		};

        if ( prov.FetchUnit( off, first ) && cp.hi_surrogate.checksum == 54 )
        {
			union
			{
				wideCharType second;
				code_point_lo_surrogate ls;
			};

            if ( prov.FetchUnit( off + 1, second ) && ls.checksum == 55 )
            {
                // We are two char16_t in size.
                return 2;
            }

            // Kinda erroneous, you know?
            throw eir::codepoint_exception( L"UTF-16 stream error: invalid surrogate sequence detected" );
        }

        // We are simply char16_t in size.
        return 1;
    }

    static AINLINE ucp_t ResolveStreamCodepoint( size_t cpsize, const wideCharType *iter )
    {
		// TODO: take into account endianness.

		union
		{
			wideCharType first;
			code_point cp;
		};

		if ( cpsize == 1 )
		{
			first = *iter;

			return cp.ucp;
		}
		else if ( cpsize == 2 )
		{
            // Decode a value outside BMP!
            // It is a 21bit int, the top five bits are the plane, then 16bits of actual plane index.
			union
			{
				wideCharType second;
				code_point_lo_surrogate ls;
			};
			first = *( iter );
			second = *( iter + 1 );

            result_code_point res;
            res.lo_order = ls.loworder_cp;
            res.hi_order = cp.hi_surrogate.hiorder_cp;
            res.plane_id = ( 1 + cp.hi_surrogate.plane_id );
            res.pad = 0;

            return res.value;
		}

		throw eir::codepoint_exception( L"UTF-16 decoding unexpected runtime error" );
    }

	static constexpr wideCharType cp_unknown = cp_unknown_param;
	static constexpr ucp_t ucp_unknown = L'_';
    static constexpr size_t ucp_max = std::numeric_limits <std::uint32_t>::max();

    struct enc_result
    {
        wideCharType data[2];
        size_t numData;
    };

    static AINLINE void EncodeUCP( ucp_t cp, enc_result& resOut )
    {
        if ( cp >= 0x10000 )
        {
            const result_code_point& codepoint = (const result_code_point&)cp;

            // Make sure the codepoint is even valid.
            if ( codepoint.pad != 0 )
            {
                throw eir::codepoint_exception( L"UTF-16 encoding exception: invalid UTF-32 codepoint" );
            }

            // We encode to two code points, basically a surrogate pair.
            union
            {
                wideCharType cp_zero;
                code_point_hi_surrogate hi_surrogate;
            };

            union
            {
                wideCharType cp_one;
                code_point_lo_surrogate lo_surrogate;
            };

            hi_surrogate.hiorder_cp = codepoint.hi_order;
            hi_surrogate.plane_id = ( codepoint.plane_id - 1 );
            hi_surrogate.checksum = 54;

            lo_surrogate.loworder_cp = codepoint.lo_order;
            lo_surrogate.checksum = 55;

            // Write the data back.
            resOut.data[0] = cp_zero;
            resOut.data[1] = cp_one;

            resOut.numData = 2;
        }
        else
        {
            // We just encode to one code point.
            resOut.data[0] = (wideCharType)cp;
            resOut.numData = 1;
        }
    }
};

template <>
struct character_env <char16_t> : public utf16_character_env <char16_t, u'_'>
{};

template <typename utf32_char_type, utf32_char_type cp_unknown_param>
struct utf32_character_env
{
    typedef char32_t ucp_t; // UNICODE CODE POINT, can represent all characters.

    // We represent UTF-32 strings here. They are the fastest Unicode strings available.
    // They are fixed length and decode 1:1, very similar to ANSI, even if not perfect.
	template <typename char_prov>
	static AINLINE size_t GetStreamCodepointSize( size_t off, const char_prov& prov )
	{
		return 1;
	}

	static AINLINE ucp_t ResolveStreamCodepoint( size_t cpsize, const utf32_char_type *iter )
	{
		// cpsize is always one.
		return *iter;
	}

	static constexpr utf32_char_type cp_unknown = cp_unknown_param;
	static constexpr ucp_t ucp_unknown = U'_';
    static constexpr size_t ucp_max = std::numeric_limits <std::uint32_t>::max();

    struct enc_result
    {
        utf32_char_type data[1];
        size_t numData;
    };

    static AINLINE void EncodeUCP( ucp_t codepoint, enc_result& resOut )
    {
        // TODO: check the UTF-32 character for validity.

        resOut.data[0] = codepoint;
        resOut.numData = 1;
    }
};

template <>
struct character_env <char32_t> : utf32_character_env <char32_t, U'_'>
{};

// Declare the special wchar_t compatibility type.
using wchar_utf_char_env = std::conditional <sizeof(wchar_t) == sizeof(char32_t), utf32_character_env <wchar_t, L'_'>, utf16_character_env <wchar_t, L'_'>>::type;

template <>
struct character_env <wchar_t> : public wchar_utf_char_env
{};

#if !defined(__cpp_char8_t)
// To still be able to use character_env (everything is tied to it),
// we invent a special UTF-8 character type.
enum class char8_t : char {};
#endif //compiler has no char8_t support.

// UTF-8 character environment.
template <>
struct character_env <char8_t>
{
	// We can store really big unicode characters.
	typedef char32_t ucp_t;

private:
	// Codepoint types of UTF-8.
	struct utf8_first_byte_one
	{
		unsigned char value : 7;
		unsigned char is_zero : 1;
	};

	struct utf8_first_byte_two
	{
		unsigned char value : 5;
		unsigned char checksum : 3;	// pattern: 110
	};

	struct utf8_first_byte_three
	{
		unsigned char value : 4;
		unsigned char checksum : 4;	// pattern: 1110
	};

	struct utf8_first_byte_four
	{
		unsigned char value : 3;
		unsigned char checksum : 5;	// pattern: 11110
	};

	struct utf8_followup_wide_byte
	{
		unsigned char value : 6;
		unsigned char checksum : 2;	// pattern: 10
	};

	// The second byte in a four-byte row.
	struct utf8_fractured_byte
	{
		unsigned char z : 4;
		unsigned char u : 2;
		unsigned char checksum : 2;	// pattern: 10
	};

	// The UCP union results.
	struct utf8_ucp_size_one
	{
		std::uint32_t x : 7;
		std::uint32_t zero : 25;
	};

	struct utf8_ucp_size_two
	{
		std::uint32_t x : 6;
		std::uint32_t y : 5;
		std::uint32_t zero : 21;
	};

	struct utf8_ucp_size_three
	{
		std::uint32_t x : 6;
		std::uint32_t y : 6;
		std::uint32_t z : 4;
		std::uint32_t zero : 16;
	};

	struct utf8_ucp_size_four
	{
		union
		{
			// Encoding friendly version.
			struct
			{
				std::uint32_t x : 6;
				std::uint32_t y : 6;
				std::uint32_t z : 4;
				std::uint32_t u_low : 2;
				std::uint32_t u_high : 3;
				std::uint32_t zero : 11;
			} enc;
			struct
			{
				std::uint32_t x : 6;
				std::uint32_t y : 6;
				std::uint32_t z : 4;
				std::uint32_t u : 5;
				std::uint32_t zero : 11;
			} valform;
		};
	};

public:
	// Calculate the size of an UTF-8 codepoint.
	template <typename char_prov>
	static AINLINE size_t GetStreamCodepointSize( size_t off, const char_prov& prov )
	{
		union
		{
			char8_t first;
			utf8_first_byte_one cp_one;
			utf8_first_byte_two cp_two;
			utf8_first_byte_three cp_three;
			utf8_first_byte_four cp_four;
		};

		// Always have to read at least one unit.
		if ( prov.FetchUnit( off, first ) )
		{
			// Check for size 1.
			if ( cp_one.is_zero == 0 )
			{
				return 1;
			}

			// Check for size 2.
			if ( cp_two.checksum == 6 && cp_two.value != 0 )
			{
				// Also has to have a follow-up byte.
				union
				{
					char8_t second;
					utf8_followup_wide_byte cp_next;
				};

				if ( prov.FetchUnit( off + 1, second ) && cp_next.checksum == 2 )
				{
					return 2;
				}
			}

			// Check for size 3.
			if ( cp_three.checksum == 14 && cp_three.value != 0 )
			{
				// Check two more follow-up bytes.
				union
				{
					char8_t second;
					utf8_followup_wide_byte first_next;
				};

				if ( prov.FetchUnit( off + 1, second ) && first_next.checksum == 2 )
				{
					union
					{
						char8_t third;
						utf8_followup_wide_byte second_next;
					};

					if ( prov.FetchUnit( off + 2, third ) && second_next.checksum == 2 )
					{
						return 3;
					}
				}
			}

			// Check for size 4.
			if ( cp_four.checksum == 30 )
			{
				// We have a special second byte here.
				union
				{
					char8_t second;
					utf8_fractured_byte frac;
				};

				if ( prov.FetchUnit( off + 1, second ) && frac.checksum == 2 )
				{
					// Check the condition that we have a valid u.
					// This ensures bijectivity of the encoding/decoding.
					if ( cp_four.value != 0 || frac.u != 0 )
					{
						// Next we just have two more normal wide bytes.
						union
						{
							char8_t third;
							utf8_followup_wide_byte second_ext;
						};

						if ( prov.FetchUnit( off + 2, third ) && second_ext.checksum == 2 )
						{
							union
							{
								char8_t fourth;
								utf8_followup_wide_byte third_ext;
							};

							if ( prov.FetchUnit( off + 3, fourth ) && third_ext.checksum == 2 )
							{
								return 4;
							}
						}
					}
				}
			}
		}

		// No valid size found; we must have encountered a malformed stream!
		throw eir::codepoint_exception( L"could not determine codepoint length; malformed stream" );
	}

	static AINLINE ucp_t ResolveStreamCodepoint( size_t cpsize, const char8_t *cur_iter )
	{
		ucp_t result_ucp;

		if ( cpsize == 1 )
		{
			// A simple character.
			const utf8_first_byte_one *cp = (const utf8_first_byte_one*)cur_iter;

			union
			{
				ucp_t res_conv;
				utf8_ucp_size_one bitfield;
			};

			bitfield.x = cp->value;
			bitfield.zero = 0;

			result_ucp = res_conv;
		}
		else if ( cpsize == 2 )
		{
			// Two characters.
			const utf8_first_byte_two *cp = (const utf8_first_byte_two*)cur_iter;
			const utf8_followup_wide_byte *second = (const utf8_followup_wide_byte*)( cp + 1 );

			union
			{
				ucp_t res_conv;
				utf8_ucp_size_two bitfield;
			};

			bitfield.x = second->value;
			bitfield.y = cp->value;
			bitfield.zero = 0;

			result_ucp = res_conv;
		}
		else if ( cpsize == 3 )
		{
			// Three bytes in total.
			const utf8_first_byte_three *cp = (const utf8_first_byte_three*)cur_iter;
			const utf8_followup_wide_byte *second = (const utf8_followup_wide_byte*)( cp + 1 );
			const utf8_followup_wide_byte *third = (const utf8_followup_wide_byte*)( second + 1 );

			union
			{
				ucp_t res_conv;
				utf8_ucp_size_three bitfield;
			};

			bitfield.x = third->value;
			bitfield.y = second->value;
			bitfield.z = cp->value;
			bitfield.zero = 0;

			result_ucp = res_conv;
		}
		else if ( cpsize == 4 )
		{
			// The ultimate code-point.
			const utf8_first_byte_four *cp = (const utf8_first_byte_four*)cur_iter;
			const utf8_fractured_byte *frac = (const utf8_fractured_byte*)( cp + 1 );
			const utf8_followup_wide_byte *third = (const utf8_followup_wide_byte*)( frac + 1 );
			const utf8_followup_wide_byte *fourth = (const utf8_followup_wide_byte*)( third + 1 );

			union
			{
				ucp_t res_conv;
				utf8_ucp_size_four bitfield;
			};

			bitfield.enc.x = fourth->value;
			bitfield.enc.y = third->value;
			bitfield.enc.z = frac->z;
			bitfield.enc.u_low = frac->u;
			bitfield.enc.u_high = cp->value;
			bitfield.enc.zero = 0;

			result_ucp = res_conv;
		}
		else
		{
			throw eir::codepoint_exception( L"unexpected runtime error in UTF-8 decoding" );
		}

		// TODO: maybe check for codepoint validity?

		return result_ucp;
	}

	static constexpr char8_t cp_unknown = u8'_';
	static constexpr ucp_t ucp_unknown = U'_';
    static constexpr size_t ucp_max = std::numeric_limits <std::uint32_t>::max();

	// We also require encoding operation.
	struct enc_result
	{
		char8_t data[4];
		size_t numData;
	};

	static AINLINE void EncodeUCP( ucp_t codepoint, enc_result& encOut )
	{
		union
		{
			ucp_t cp_data;
			utf8_ucp_size_one bitfield_one;
			utf8_ucp_size_two bitfield_two;
			utf8_ucp_size_three bitfield_three;
			utf8_ucp_size_four bitfield_four;
		};

		cp_data = codepoint;

		if ( bitfield_one.zero == 0 )
		{
			union
			{
				utf8_first_byte_one first;
				char8_t first_char;
			};
			first.value = bitfield_one.x;
			first.is_zero = 0;

			encOut.data[0] = first_char;
			encOut.numData = 1;
		}
		else if ( bitfield_two.y != 0 && bitfield_two.zero == 0 )
		{
			union
			{
				utf8_first_byte_two first;
				char8_t first_char;
			};
			first.value = bitfield_two.y;
			first.checksum = 6;

			union
			{
				utf8_followup_wide_byte second;
				char8_t second_char;
			};
			second.value = bitfield_two.x;
			second.checksum = 2;

			encOut.data[0] = first_char;
			encOut.data[1] = second_char;
			encOut.numData = 2;
		}
		else if ( bitfield_three.z != 0 && bitfield_three.zero == 0 )
		{
			union
			{
				utf8_first_byte_three first;
				char8_t first_char;
			};
			first.value = bitfield_three.z;
			first.checksum = 14;

			union
			{
				utf8_followup_wide_byte second;
				char8_t second_char;
			};
			second.value = bitfield_three.y;
			second.checksum = 2;

			union
			{
				utf8_followup_wide_byte third;
				char8_t third_char;
			};
			third.value = bitfield_three.x;
			third.checksum = 2;

			encOut.data[0] = first_char;
			encOut.data[1] = second_char;
			encOut.data[2] = third_char;
			encOut.numData = 3;
		}
		else if ( bitfield_four.valform.u != 0 && bitfield_four.valform.zero == 0 )
		{
			union
			{
				utf8_first_byte_four first;
				char8_t first_char;
			};
			first.value = bitfield_four.enc.u_high;
			first.checksum = 30;

			union
			{
				utf8_fractured_byte second;
				char8_t second_char;
			};
			second.u = bitfield_four.enc.u_low;
			second.z = bitfield_four.enc.z;
			second.checksum = 2;

			union
			{
				utf8_followup_wide_byte third;
				char8_t third_char;
			};
			third.value = bitfield_four.enc.y;
			third.checksum = 2;

			union
			{
				utf8_followup_wide_byte fourth;
				char8_t fourth_char;
			};
			fourth.value = bitfield_four.enc.x;
			fourth.checksum = 2;

			encOut.data[0] = first_char;
			encOut.data[1] = second_char;
			encOut.data[2] = third_char;
			encOut.data[3] = fourth_char;
			encOut.numData = 4;
		}
		else
		{
			throw eir::codepoint_exception( L"failed to encode code-point; invalid code-point value" );
		}
	}
};

// Helper function for correctly transforming UCP values from one environment to another.
template <typename src_env, typename dst_env>
AINLINE bool BoolAcquireDirectUCP( typename src_env::ucp_t src_ucp, typename dst_env::ucp_t& dst_ucp )
{
    auto unsigned_ucp = (typename std::make_unsigned <typename src_env::ucp_t>::type)src_ucp;

    if ( unsigned_ucp < dst_env::ucp_max )
    {
        dst_ucp = (typename dst_env::ucp_t)unsigned_ucp;
        return true;
    }

    return false;
}

template <typename src_env, typename dst_env>
AINLINE typename dst_env::ucp_t AcquireDirectUCP( typename src_env::ucp_t src_ucp )
{
	auto unsigned_ucp = (typename std::make_unsigned <typename src_env::ucp_t>::type)src_ucp;

	if ( unsigned_ucp < dst_env::ucp_max )
	{
		return (typename dst_env::ucp_t)unsigned_ucp;
	}
	else
	{
		return dst_env::ucp_unknown;
	}
}

// Case-sensitivity algorithms and databases.
#include "UniChar.casesense.h"

// Character comparison for sorting.
template <typename leftCharType, typename rightCharType>
AINLINE eir::eCompResult CompareCharacterEx(
    leftCharType left, rightCharType right,
    const toupper_lookup <leftCharType>& leftFacet, const toupper_lookup <rightCharType>& rightFacet,
    bool caseSensitive
)
{
    leftCharType real_left;
    rightCharType real_right;

    if ( caseSensitive )
    {
        real_left = left;
        real_right = right;
    }
    else
    {
        real_left = leftFacet.toupper( left );
        real_right = rightFacet.toupper( right );
    }

    auto unsigned_real_left = (typename std::make_unsigned <leftCharType>::type)real_left;
    auto unsigned_real_right = (typename std::make_unsigned <rightCharType>::type)real_right;

    if ( unsigned_real_left < unsigned_real_right )
    {
        return eir::eCompResult::LEFT_LESS;
    }

    if ( unsigned_real_left > unsigned_real_right )
    {
        return eir::eCompResult::LEFT_GREATER;
    }

    return eir::eCompResult::EQUAL;
}

// Quick helper with less performance.
template <typename leftCharType, typename rightCharType>
AINLINE eir::eCompResult CompareCharacter(
    leftCharType left, rightCharType right,
    bool caseSensitive
)
{
    const std::locale& classic_loc = std::locale::classic();

    toupper_lookup <leftCharType> leftFacet( classic_loc );
    toupper_lookup <rightCharType> rightFacet( classic_loc );

    return CompareCharacterEx( left, right, leftFacet, rightFacet, caseSensitive );
}

// Extended specialized cached character comparison.
template <typename leftCharType, typename rightCharType>
AINLINE bool IsCharacterEqualEx(
    leftCharType left, rightCharType right,
    const toupper_lookup <leftCharType>& leftFacet, const toupper_lookup <rightCharType>& rightFacet,
    bool caseSensitive
)
{
    return ( CompareCharacterEx( left, right, leftFacet, rightFacet, caseSensitive ) == eir::eCompResult::EQUAL );
}

// The main function of comparing characters.
template <typename leftCharType, typename rightCharType>
AINLINE bool IsCharacterEqual(
    leftCharType left, rightCharType right,
    bool caseSensitive
)
{
    const std::locale& classic_loc = std::locale::classic();

    toupper_lookup <leftCharType> leftFacet( classic_loc );
    toupper_lookup <rightCharType> rightFacet( classic_loc );

    return IsCharacterEqualEx( left, right, leftFacet, rightFacet, caseSensitive );
}

#include "UniChar.strmodel.h"

namespace CharacterUtil
{
    template <eir::CharacterType charType>
    AINLINE const charType* GetDefaultConfErrorString( void )
    {
		if constexpr ( std::is_same <charType, char>::value )
		{
			return "<codepoint_error>";
		}
		else if constexpr ( std::is_same <charType, wchar_t>::value )
		{
			return L"<codepoint_error>";
		}
		else if constexpr ( std::is_same <charType, char8_t>::value )
		{
			return (const char8_t*)u8"<codepoint_error>";
		}
		else if constexpr ( std::is_same <charType, char16_t>::value )
		{
			return u"<codepoint_error>";
		}
		else if constexpr ( std::is_same <charType, char32_t>::value )
		{
			return U"<codepoint_error>";
		}
    }

    template <typename inputCharType, typename outputCharType, typename... stringArgs>
    AINLINE void TranscodeCharacter( typename character_env <inputCharType>::ucp_t ucp, eir::String <outputCharType, stringArgs...>& output_str )
    {
        typedef character_env <inputCharType> input_env;
        typedef character_env <outputCharType> output_env;

        // Encode it into the output string, if possible.
        typename output_env::ucp_t enc_codepoint = AcquireDirectUCP <input_env, output_env> ( ucp );

        typename output_env::enc_result result;

        output_env::EncodeUCP( enc_codepoint, result );

        for ( size_t n = 0; n < result.numData; n++ )
        {
            output_str += result.data[ n ];
        }
    }

	template <typename inputCharType, typename outputCharType, typename char_prov_t, typename... stringParams>
	inline void _TranscodeStringWithCharProv( const inputCharType *inputChars, char_prov_t&& char_prov, eir::String <outputCharType, stringParams...>& output_str )
	{
		// TODO: optimize this function so that we reserve as many code-points into output_str as
		//  we would put into it by using a fast scan.

        typedef character_env <inputCharType> input_env;
        //typedef character_env <outputCharType> output_env;

        character_env_iterator <inputCharType, decltype(char_prov)> iter( inputChars, std::move( char_prov ) );

        while ( !iter.IsEnd() )
        {
            typename input_env::ucp_t codepoint = iter.ResolveAndIncrement();

            TranscodeCharacter <inputCharType> ( codepoint, output_str );
        }
	}

	// Helpers for simplicity.
	template <typename inputCharType, typename outputCharType, typename... stringParams>
	inline void TranscodeStringToZero( const inputCharType *inputChars, eir::String <outputCharType, stringParams...>& output_str )
	{
		_TranscodeStringWithCharProv( inputChars, charenv_charprov_tozero <inputCharType> ( inputChars ), output_str );
	}

	template <typename inputCharType, typename outputCharType, typename... stringParams>
	inline void TranscodeStringLength( const inputCharType *inputChars, size_t inputLength, eir::String <outputCharType, stringParams...>& output_str )
	{
		_TranscodeStringWithCharProv( inputChars, charenv_charprov_tocplen <inputCharType> ( inputChars, inputLength ), output_str );
	}

    template <typename inputCharType, typename outputCharType, eir::MemoryAllocator allocatorType, typename exceptMan = eir::DefaultExceptionManager, typename... Args>
    inline eir::String <outputCharType, allocatorType, exceptMan> ConvertStrings( const inputCharType *inputChars, Args&&... allocArgs )
    {
        try
        {
            eir::String <outputCharType, allocatorType, exceptMan> output_str( nullptr, 0, std::forward <Args> ( allocArgs )... );

            // Convert things into the output.
            {
				charenv_charprov_tozero <inputCharType> char_prov( inputChars );

				_TranscodeStringWithCharProv( inputChars, std::move( char_prov ), output_str );
            }

            return output_str;
        }
        catch( eir::codepoint_exception& )
        {
            // On error we just return an error string.
            return GetDefaultConfErrorString <outputCharType> ();
        }
    }

    template <typename inputCharType, typename outputCharType, eir::MemoryAllocator allocatorType, bool optimizeEqual = true, typename exceptMan = eir::DefaultExceptionManager, typename... Args>
    inline eir::String <outputCharType, allocatorType, exceptMan> ConvertStringsLength( const inputCharType *inputChars, size_t inputLen, Args&&... allocArgs )
    {
        if constexpr ( optimizeEqual && std::is_same <inputCharType, outputCharType>::value )
        {
            return eir::String <outputCharType, allocatorType, exceptMan> ( inputChars, inputLen, std::forward <Args> ( allocArgs )... );
        }
        else
        {
            eir::String <outputCharType, allocatorType, exceptMan> output_str( nullptr, 0, std::forward <Args> ( allocArgs )... );

            try
            {
                // Convert things into the output.
                {
				    charenv_charprov_tocplen <inputCharType> char_prov( inputChars, inputLen );

				    _TranscodeStringWithCharProv( inputChars, std::move( char_prov ), output_str );
                }

                return output_str;
            }
            catch( eir::codepoint_exception& )
            {
                // On error we just return an error string.
                output_str = GetDefaultConfErrorString <outputCharType> ();
                return output_str;
            }
        }
    }

    template <typename inputCharType, typename outputCharType, eir::MemoryAllocator allocatorType, bool optimizeEqual = true, typename exceptMan, typename... Args>
    inline eir::String <outputCharType, allocatorType, exceptMan> ConvertStrings( const eir::String <inputCharType, allocatorType, exceptMan>& inputStr, Args&&... allocArgs )
    {
        if constexpr ( optimizeEqual && std::is_same <inputCharType, outputCharType>::value )
        {
            return inputStr;
        }
        else
        {
            return ConvertStringsLength <inputCharType, outputCharType, allocatorType, false, exceptMan> ( inputStr.GetConstString(), inputStr.GetLength(), std::forward <Args> ( allocArgs )... );
        }
    }

	template <typename inputCharType, typename outputCharType, eir::MemoryAllocator allocatorType, typename exceptMan = eir::DefaultExceptionManager, typename... Args>
	inline eir::String <outputCharType, allocatorType, exceptMan> ToUpperByCodepoint( const inputCharType *inputStr, Args&&... allocArgs )
	{
		eir::String <outputCharType, allocatorType, exceptMan> result( eir::constr_with_alloc::DEFAULT, std::forward <Args> ( allocArgs )... );

		character_env_iterator_tozero iter( inputStr );

		typedef typename character_env <inputCharType>::ucp_t ucp_t;

		toupper_lookup <ucp_t> tu_lkup( std::locale::classic() );

		while ( !iter.IsEnd() )
		{
			auto ucp = iter.ResolveAndIncrement();

			auto upper_ucp = tu_lkup.toupper( ucp );

			TranscodeCharacter <ucp_t, outputCharType> ( upper_ucp, result );
		}

		return result;
	}

	template <typename inputCharType, typename outputCharType, eir::MemoryAllocator allocatorType, typename exceptMan = eir::DefaultExceptionManager, typename... Args>
	inline eir::String <outputCharType, allocatorType, exceptMan> ToUpperByCodepoint( const eir::FixedString <inputCharType>& inputStr, Args&&... allocArgs )
	{
		eir::String <outputCharType, allocatorType, exceptMan> result( eir::constr_with_alloc::DEFAULT, std::forward <Args> ( allocArgs )... );

		const inputCharType *inputPtr = inputStr.GetConstString();

		charenv_charprov_tocplen prov( inputPtr, inputStr.GetLength() );
		character_env_iterator iter( inputPtr, std::move( prov ) );

		typedef typename character_env <inputCharType>::ucp_t ucp_t;

		toupper_lookup <ucp_t> tu_lkup( std::locale::classic() );

		while ( !iter.IsEnd() )
		{
			auto ucp = iter.ResolveAndIncrement();

			auto upper_ucp = tu_lkup.toupper( ucp );

			TranscodeCharacter <ucp_t, outputCharType> ( upper_ucp, result );
		}

		return result;
	}
	
	template <typename inputCharType, typename outputCharType, eir::MemoryAllocator allocatorType, typename exceptMan, typename... Args>
	inline eir::String <outputCharType, allocatorType, exceptMan> ToUpperByCodepoint( const eir::String <inputCharType, allocatorType, exceptMan>& inputStr, Args&&... allocArgs )
	{
		return ToUpperByCodepoint <inputCharType, outputCharType, allocatorType, exceptMan> ( inputStr.ToFixed(), std::forward <Args> ( allocArgs )... );
	}

	template <typename inputCharType, typename outputCharType, eir::MemoryAllocator allocatorType, typename exceptMan = eir::DefaultExceptionManager, typename... Args>
	inline eir::String <outputCharType, allocatorType, exceptMan> ToLowerByCodepoint( const inputCharType *inputStr, Args&&... allocArgs )
	{
		eir::String <outputCharType, allocatorType, exceptMan> result( eir::constr_with_alloc::DEFAULT, std::forward <Args> ( allocArgs )... );

		character_env_iterator_tozero iter( inputStr );

		typedef typename character_env <inputCharType>::ucp_t ucp_t;

		toupper_lookup <ucp_t> tu_lkup( std::locale::classic() );

		while ( !iter.IsEnd() )
		{
			auto ucp = iter.ResolveAndIncrement();

			auto lower_ucp = tu_lkup.tolower( ucp );

			TranscodeCharacter <ucp_t, outputCharType> ( lower_ucp, result );
		}

		return result;
	}

	template <typename inputCharType, typename outputCharType, eir::MemoryAllocator allocatorType, typename exceptMan = eir::DefaultExceptionManager, typename... Args>
	inline eir::String <outputCharType, allocatorType, exceptMan> ToLowerByCodepoint( const eir::FixedString <inputCharType>& inputStr, Args&&... allocArgs )
	{
		eir::String <outputCharType, allocatorType, exceptMan> result( eir::constr_with_alloc::DEFAULT, std::forward <Args> ( allocArgs )... );

		const inputCharType *inputPtr = inputStr.GetConstString();

		charenv_charprov_tocplen prov( inputPtr, inputStr.GetLength() );
		character_env_iterator iter( inputPtr, std::move( prov ) );

		typedef typename character_env <inputCharType>::ucp_t ucp_t;

		toupper_lookup <ucp_t> tu_lkup( std::locale::classic() );

		while ( !iter.IsEnd() )
		{
			auto ucp = iter.ResolveAndIncrement();

			auto lower_ucp = tu_lkup.tolower( ucp );

			TranscodeCharacter <ucp_t, outputCharType> ( lower_ucp, result );
		}

		return result;
	}
	
	template <typename inputCharType, typename outputCharType, eir::MemoryAllocator allocatorType, typename exceptMan, typename... Args>
	inline eir::String <outputCharType, allocatorType, exceptMan> ToLowerByCodepoint( const eir::String <inputCharType, allocatorType, exceptMan>& inputStr, Args&&... allocArgs )
	{
		return ToLowerByCodepoint <inputCharType, outputCharType, allocatorType, exceptMan> ( inputStr.ToFixed(), std::forward <Args> ( allocArgs )... );
	}
};

#endif //_EIRREPO_CHARACTER_RESOLUTION_
