/*
 *	Copyright (C) 2009-2010 K.R.F. Studio.
 *
 *	$Id: ddns_string.c 243 2010-07-13 14:10:07Z karl $
 *
 *	This file is part of 'ddns', Created by karl on 2009-09-25.
 *
 *	'ddns' is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published
 *	by the Free Software Foundation; either version 3 of the License,
 *	or (at your option) any later version.
 *
 *	'ddns' is distributed in the hope that it will be useful, but
 *	WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with 'ddns'; if not, write to the Free Software Foundation, Inc.,
 *	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/*
 *  Provide ISO-C99 compatible string functions. For compilers that
 *	compatible with ISO-C99 standard, it just simply redirect the calls
 *	to C runtime. This file presents here because of some brain-dead
 *	compilers.
 */

#include "ddns_string.h"
#include <math.h>	/* C89: ceil */
#include <assert.h>	/* C89: assert */
#include <string.h>	/* C89: strlen */
#include <stdlib.h>	/* C89: strtol */
#include "ddns_types.h"

#define FLAG_NONE			0x00000000	/* no flag specified */
#define FLAG_LEFTJUSTIFY	0x00000100	/* flag '-' */
#define FLAG_SIGNCONV		0x00000200	/* flag '+' */
#define FLAG_SPACE			0x00000400	/* flag ' ' */
#define FLAG_ALTFORM		0x00000800	/* flag '#' */
#define FLAG_PREFIX0		0x00001000	/* flag '0' */

#define	LENGTH_INT			0x00000000	/* no length modifier   */
#define LENGTH_CHAR			0x00010000	/* length modifier 'hh' */
#define LENGTH_SHRT			0x00020000	/* length modifier 'h'  */
#define LENGTH_LONG			0x00040000	/* length modifier 'l'  */
#define LENGTH_LLONG		0x00080000	/* length modifier 'll' */
#define LENGTH_INTMAX		0x00100000	/* length modifier 'j'  */
#define LENGTH_SIZE			0x00200000	/* length modifier 'z'  */
#define LENGTH_PTRDIFF		0x00400000	/* length modifier 't'  */
#define LENGTH_LDOUBLE		0x00800000	/* length modifier 'L'	*/


/**
 *	Output a character & do buffer overflow check.
 *
 *	@param[out]	str		: the buffer.
 *	@param[in]	size	: buffer size.
 *	@param[in]	pos		: position to output the character.
 *	@param[in]	chr		: the character to output.
 *
 *	@note	It will always preserve space for a null terminator in [str] before
 *			writing data to it.
 *
 *	@return		Number of character actually wrote to the output buffer.
 */
static int output_chr(XCHAR* str, size_t size, size_t pos, XCHAR chr);


/**
 *	Format a integer parameter.
 *
 *	@param[out]	str			: the buffer to output.
 *	@param[in]	size		: size of the buffer to output.
 *	@param[in]	pos			: position to write formated string in the buffer.
 *	@param[out]	true_size	: number of characters actually wrote to the buffer.
 *	@param[in]	value		: the number to be formated.
 *	@param[in]	is_neg		: whether the number is a negative one.
 *	@param[in]	flags		: fprintf flags.
 *	@param[in]	precision	: value precision when formating the value.
 *	@param[in]	width		: minimum width of the integer field.
 *	@param[in]	base		:
 *	@param[in]	signed_int	: whether the value is signed.
 *	@param[in]	capital		: whether use capital character to format hex value.
 *
 *	@return		Number of characters should have been wrote if the buffer is
 *				sufficiently large.
 */
static size_t format_int(
	XCHAR	*		str,
	size_t			size,
	size_t			pos,
	size_t	*		true_size,
	ddns_uintmax	value,
	int				is_neg,
	int				flags,
	int				precision,
	int				width,
	int				base,
	int				capital
	);


/**
 *	Format a integer parameter.
 *
 *	@param[out]	str			: the buffer to output.
 *	@param[in]	size		: size of the buffer to output.
 *	@param[in]	pos			: position to write formated string in the buffer.
 *	@param[out]	true_size	: number of characters actually wrote to the buffer.
 *	@param[out]	field_size	: number of characters should have been wrote if the
 *							  buffer is sufficiently large.
 *	@param[in]	value		: the value to be formated.
 *	@param[in]	flags		: fprintf flags.
 *	@param[in]	precision	: value precision when formating the value.
 *	@param[in]	length		: 'length modifier' of the argument.
 *	@param[in]	width		: minimum width of the integer field.
 *	@param[in]	format		: the conversion specifier.
 *
 *	@return		required buffer size to format the value, not including null
 *				terminator.
 */
static size_t format_double(
	XCHAR		*	str,
	size_t			size,
	size_t			pos,
	size_t		*	true_size,
	long double		value,
	int				flags,
	int				precision,
	int				length,
	int				width,
	XCHAR			format
	);


/**
 *	Format a string parameter.
 *
 *	@param[out]	str			: the buffer to output.
 *	@param[in]	size		: size of the buffer to output.
 *	@param[in]	pos			: position to write formated string in the buffer.
 *	@param[out]	true_size	: number of characters actually wrote to the buffer.
 *	@param[in]	value		: the string to be formated.
 *	@param[in]	flags		: fprintf flags.
 *	@param[in]	precision	: value precision when formating the value.
 *	@param[in]	length		: 'length modifier' of the argument.
 *	@param[in]	width		: minimum width of the integer field.
 *
 *	@return		Number of characters should have been wrote if the buffer is
 *				sufficiently large.
 */
static size_t format_string(
	XCHAR	*		str,
	size_t			size,
	size_t			pos,
	size_t	*		true_size,
	const XCHAR	*	value,
	int				flags,
	int				precision,
	int				length,
	int				width
	);


/**
 *	Format a string parameter.
 *
 *	@param[out]	str			: the buffer to output.
 *	@param[in]	size		: size of the buffer to output.
 *	@param[in]	pos			: position to write formated string in the buffer.
 *	@param[out]	true_size	: number of characters actually wrote to the buffer.
 *	@param[out]	field_size	: number of characters should have been wrote if the
 *							  buffer is sufficiently large.
 *	@param[in]	ap			: the parameter list.
 *	@param[in]	flags		: fprintf flags.
 *	@param[in]	length		: 'length modifier' of the argument.
 *	@param[in]	width		: minimum width of the integer field.
 *
 *	@return		Number of characters should have been wrote if the buffer is
 *				sufficiently large.
 */
static size_t format_character(
	XCHAR	*	str,
	size_t		size,
	size_t		pos,
	size_t	*	true_size,
	XCHAR		value,
	int			flags,
	int			length,
	int			width
	);


/**
 *	Output sign mark and 'alternative form' prefix.
 *
 *	@param[out]	str			: the buffer to output.
 *	@param[in]	size		: size of the buffer to output.
 *	@param[in]	pos			: position to write formated string in the buffer.
 *	@param[out]	true_size	: number of characters actually wrote to the buffer.
 *	@param[in]	is_negative	: whether output '-' in the buffer.
 *	@param[in]	flags		: fprintf flags.
 *	@param[in]	base		:
 *	@param[in]	capital		: whether use capital character.
 *
 *	@return		Number of characters should have been wrote to the buffer if
 *				it's sufficiently large.
 */
static int format_int_prefix(
	XCHAR	*	str,
	size_t		size,
	size_t		pos,
	size_t	*	true_size,
	int			is_negative,
	int			flags,
	int			base,
	int			capital
	);


/**
 *	ISO-C99 "strncpy" replacement.
 */
XCHAR * c99_strncpy(XCHAR *dst, const XCHAR *src, size_t size)
{
	XCHAR * _dst = dst;

	if ( (NULL != dst) && (size > 0) )
	{
		--size;	/* reserve space for NULL-terminator */

		if ( NULL != src )
		{
			for ( ; (size > 0) && ('\0' != *src); _xchar_inc(src), --size )
			{
				*dst = *src;
				_xchar_inc(dst);
			}
		}

		(*dst) = '\0';
	}

	return _dst;
}


/**
 *	ISO-C99 compatible "strncat" replacement, concatenate strings.
 *
 *	@param[out]		dst		: the string to be appended to.
 *	@param[in]		src		: the string to be appended.
 *	@param[in]		size	: size of [dst] in characters.
 *
 *	@return		It returns the value of [dst].
 */
XCHAR * c99_strncat(XCHAR *dst, const XCHAR *src, size_t size)
{
	XCHAR * _dst = dst;

	if ( (NULL != dst) && (size > 0) && (NULL != src) )
	{
		--size;	/* reserve space for NULL-terminator */

		/* find the end of string [dst] */
		for ( ; (size > 0) && ('\0' != *dst); _xchar_inc(dst), --size )
		{
		}

		/* append the [src] to the end of string [dst] */
		for ( ; (size > 0) && ('\0' != *src); _xchar_inc(src), --size )
		{
			*dst = *src;
			_xchar_inc(dst);
		}

		(*dst) = '\0';
	}

	return _dst;
}


/**
 *	ISO-C99 compatible "snprintf" replacement, output formatted string.
 *
 *	@param[out]		str		: storage location for output.
 *	@param[in]		size	: maximum number of characters to write.
 *	@param[in]		format	: format specification.
 *	@param[in]		...		: optional arguments
 *
 *	@return		Number of characters (exclude '\0') should have been wrote to
 *				[str] if it's sufficiently large.
 */
int c99_snprintf(XCHAR *str, size_t size, const XCHAR *format, ...)
{
	int			retval = -1;
	va_list		ap;

	va_start(ap, format);
	retval = c99_vsnprintf(str, size, format, ap);
	va_end(ap);

	return retval;
}


/**
 *	ISO-C99 compatible "vsnprintf" replacement, output formatted string.
 *
 *	@param[out]		str		: storage location for output.
 *	@param[in]		size	: maximum number of characters to write.
 *	@param[in]		format	: format specification.
 *	@param[in]		ap		: argument list
 *
 *	@return		Number of characters (exclude '\0') should have been wrote to
 *				[str] if it's sufficiently large.
 */
int c99_vsnprintf(XCHAR *str, size_t size, const XCHAR *format, va_list ap)
{
	int				flags		= FLAG_NONE;/* fprintf: flags             */
	int				width		= 0;		/* fprintf: field width       */
	int				precision	= -1;		/* fprintf:	precision         */
	int				length		= -1;		/* fprintf: length modifier   */

	int				total_size	= 0;		/* expected buffer size       */
	int				true_size	= 0;		/* characters wrote to buffer */

	const XCHAR	*	lpsz		= format;	/* output format pointer      */
	va_list			ap_save;				/* original parameter list    */

#ifdef va_copy
	va_copy(ap_save, ap);	/* ISO-C99 */
#elif defined(__va_copy)
	__va_copy(ap_save, ap);	/* ISO-C99 proposal */
#else
	ap_save = ap;
#endif

	/**
	 *	1. Zero or more flags (in any order) that modify the meaning of the
	 *	   conversion specification.
	 *	2. An optional minimum field width.
	 *	3. An optional precision that gives the minimum number of digits to
	 *	   appear for the d, i, o, u, x, and X conversions, the number of digits
	 *	   to appear after the decimal-point character for a, A, e, E, f, and F
	 *	   conversions, the maximum number of significant digits for the g and
	 *	   G conversions, or the maximum number of bytes to be written for s
	 *	   conversions.
	 *	4. An optional length modifier that specifies the size of the argument.
	 *	5. A conversion specifier character that specifies the type of
	 *	   conversion to be applied.
	 */
	for ( ; '\0' != (*lpsz); _xchar_inc(lpsz) )
	{
		size_t	fsize		= 0;		/* expected size of a field   */
		size_t	true_fsize	= 0;		/* actual size of a field     */

		if ( '%' != (*lpsz) )
		{
			true_size += output_chr(str, size, total_size, *lpsz);
			++total_size;
			continue;
		}
		else
		{
			_xchar_inc(lpsz);		/* skip '%' */

			if ( '%' == (*lpsz) )
			{
				true_size += output_chr(str, size, total_size, '%');
				++total_size;
				continue;
			}

			flags		= FLAG_NONE;
			width		= 0;
			precision	= -1;
			length		= -1;
		}

		/**
		 * Step 1: handle 'flags'
		 */
		for( ; '\0' != (*lpsz); _xchar_inc(lpsz) )
		{
			int is_flag = 1;

			switch(*lpsz)
			{
			case '-':
				flags |= FLAG_LEFTJUSTIFY;
				break;

			case '+':
				flags |= FLAG_SIGNCONV;
				break;

			case ' ':
				flags |= FLAG_SPACE;
				break;

			case '#':
				flags |= FLAG_ALTFORM;
				break;

			case '0':
				flags |= FLAG_PREFIX0;
				break;

			default:
				/* hit non-flag character */
				is_flag = 0;
				break;
			}

			if ( 0 == is_flag )
			{
				break;
			}
		}
		/* If the space and + flags both appear, the space flag is ignored. */
		if ( 0 != (flags & FLAG_SIGNCONV) )
		{
			flags &= (~FLAG_SPACE);
		}
		/* If the '0' and '-' flags both appear, the 0 flag is ignored. */
		if ( 0 != (flags & FLAG_LEFTJUSTIFY) )
		{
			flags &= (~FLAG_PREFIX0);
		}

		/**
		 * Step 2: handle 'field width'
		 */
		if ( '*' == (*lpsz) )
		{
			_xchar_inc(lpsz);
			width = va_arg(ap, int);
		}
		else
		{
			XCHAR * endchr = NULL;
			width = strtol(lpsz, &endchr, 10);	/* C89 required */
			lpsz = endchr;
		}
		if ( width < 0 )
		{
			/**
			 * A negative field width argument is taken as a - flag followed
			 * by a positive field width.
			 */
			width = -width;
			flags |= FLAG_LEFTJUSTIFY;
		}

		/**
		 *	Step 3: handle 'precision'.
		 */
		if ( '.' == (*lpsz) )
		{
			_xchar_inc(lpsz);	/* skip '.' */

			if ( '*' == (*lpsz) )
			{
				_xchar_inc(lpsz);
				precision = va_arg(ap, int);
			}
			else
			{
				XCHAR * endchr = NULL;
				precision = strtol(lpsz, &endchr, 10);
				lpsz = endchr;
			}

			if ( precision < 0 )
			{
				/**
				 *	A negative precision argument is taken as if the precision
				 *	were omitted.
				 */
				precision = -1;
			}
		}

		/**
		 *	Step 4: handle 'length modifier'
		 */
		switch(*lpsz)
		{
		case 'h':
			_xchar_inc(lpsz);
			if ( 'h' == (*lpsz) )
			{
				_xchar_inc(lpsz);
				length = LENGTH_CHAR;
			}
			else
			{
				length = LENGTH_SHRT;
			}
			break;

		case 'l':
			_xchar_inc(lpsz);
			if ( 'l' == (*lpsz) )
			{
				_xchar_inc(lpsz);
				length = LENGTH_LLONG;
			}
			else
			{
				length = LENGTH_LONG;
			}
			break;

		case 'j':
			_xchar_inc(lpsz);
			length = LENGTH_INTMAX;
			break;

		case 'z':
			_xchar_inc(lpsz);
			length = LENGTH_SIZE;
			break;

		case 't':
			_xchar_inc(lpsz);
			length = LENGTH_PTRDIFF;
			break;

		case 'L':
			_xchar_inc(lpsz);
			length = LENGTH_LDOUBLE;
			break;

		default:
			length = LENGTH_INT;
			break;
		}

		/**
		 * Step 5: handle 'conversion specifier'
		 */
		switch((*lpsz) | length)
		{
		case 'd' | LENGTH_CHAR:
		case 'i' | LENGTH_CHAR:
			{
				signed char		v		= (signed char)va_arg(ap, int);
				int				is_neg	= (v < 0 ? 1 : 0);
				fsize = format_int(str, size, true_size, &true_fsize, v, is_neg, flags, precision, width, 10, 0);
			}
			break;

		case 'd' | LENGTH_SHRT:
		case 'i' | LENGTH_SHRT:
			{
				signed short	v		= (signed short)va_arg(ap, int);
				int				is_neg	= (v < 0 ? 1 : 0);
				fsize = format_int(str, size, true_size, &true_fsize, v, is_neg, flags, precision, width, 10, 0);
			}
			break;
			
		case 'd' | LENGTH_INT:
		case 'i' | LENGTH_INT:
			{
				signed int		v		= va_arg(ap, signed int);
				int				is_neg	= (v < 0 ? 1 : 0);
				fsize = format_int(str, size, true_size, &true_fsize, v, is_neg, flags, precision, width, 10, 0);
			}
			break;
			
		case 'd' | LENGTH_LONG:
		case 'i' | LENGTH_LONG:
			{
				signed long 	v		= va_arg(ap, signed long);
				int				is_neg	= (v < 0 ? 1 : 0);
				fsize = format_int(str, size, true_size, &true_fsize, v, is_neg, flags, precision, width, 10, 0);
			}
			break;
			
		case 'd' | LENGTH_LLONG:
		case 'i' | LENGTH_LLONG:
#if defined(LLONG_MAX)
			{
				signed long long 	v		= va_arg(ap, signed long long);
				int					is_neg	= (v < 0 ? 1 : 0);
				fsize = format_int(str, size, true_size, &true_fsize, v, is_neg, flags, precision, width, 10, 0);
			}
#else
			assert(0);
#endif
			break;

		case 'o' | LENGTH_CHAR:
			{
				unsigned char	v		= (unsigned char)va_arg(ap, int);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 8, 0);
			}
			break;
			
		case 'o' | LENGTH_SHRT:
			{
				unsigned short	v		= (unsigned short)va_arg(ap, int);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 8, 0);
			}
			break;
			
		case 'o' | LENGTH_INT:
			{
				unsigned int	v		= va_arg(ap, unsigned int);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 8, 0);
			}
			break;
			
		case 'o' | LENGTH_LONG:
			{
				unsigned long	v		= va_arg(ap, unsigned long);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 8, 0);
			}
			break;
			
		case 'o' | LENGTH_LLONG:
#if defined(LLONG_MAX)
			{
				unsigned long long	v = va_arg(ap, unsigned long long);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 8, 0);
			}
#else
			assert(0);
#endif
			break;

		case 'u' | LENGTH_CHAR:
			{
				unsigned char	v		= (unsigned char)va_arg(ap, int);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 10, 0);
			}
			break;
			
		case 'u' | LENGTH_SHRT:
			{
				unsigned short	v		= (unsigned short)va_arg(ap, int);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 10, 0);
			}
			break;
			
		case 'u' | LENGTH_INT:
			{
				unsigned int	v		= va_arg(ap, signed int);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 10, 0);
			}
			break;
			
		case 'u' | LENGTH_LONG:
			{
				unsigned long	v		= va_arg(ap, signed long);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 10, 0);
			}
			break;
			
		case 'u' | LENGTH_LLONG:
#if defined(LLONG_MAX)
			{
				unsigned long long	v	= va_arg(ap, signed long long);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 10, 0);
			}
#else
			assert(0);
#endif
			break;

		case 'x' | LENGTH_CHAR:
			{
				unsigned char	v	= (unsigned char)va_arg(ap, int);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 16, 0);
			}
			break;
			
		case 'x' | LENGTH_SHRT:	
			{
				unsigned short	v	= (unsigned short)va_arg(ap, int);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 16, 0);
			}
			break;

		case 'x' | LENGTH_INT:	
			{
				unsigned int	v	= va_arg(ap, unsigned int);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 16, 0);
			}
			break;
			
		case 'x' | LENGTH_LONG:	
			{
				unsigned long	v	= va_arg(ap, unsigned long);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 16, 0);
			}
			break;

		case 'x' | LENGTH_LLONG:
#if defined(LLONG_MAX)
			{
				unsigned long long	v	= va_arg(ap, unsigned long long);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 16, 0);
			}
#else
			assert(0);
#endif
			break;

		case 'X' | LENGTH_CHAR:
			{
				unsigned char	v	= (unsigned char)va_arg(ap, int);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 16, 1);
			}
			break;

		case 'X' | LENGTH_SHRT:
			{
				unsigned short	v	= (unsigned short)va_arg(ap, int);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 16, 1);
			}
			break;
			
		case 'X' | LENGTH_INT:
			{
				unsigned int	v	= va_arg(ap, unsigned int);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 16, 1);
			}
			break;
			
		case 'X' | LENGTH_LONG:
			{
				unsigned long	v	= va_arg(ap, unsigned long);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 16, 1);
			}
			break;
			
		case 'X' | LENGTH_LLONG:
#if defined(LLONG_MAX)
			{
				unsigned long long	v	= va_arg(ap, unsigned long long);
				fsize = format_int(str, size, true_size, &true_fsize, v, 0, flags, precision, width, 16, 1);
			}
#else
			assert(0);
#endif
			break;

		case 's':
			{
				const XCHAR* v = va_arg(ap, const XCHAR*);
				fsize = format_string(str, size, true_size, &true_fsize, v, flags, precision, length, width);
			}
			break;

		case 'c':
			{
				XCHAR v = (XCHAR)va_arg(ap, int);
				fsize = format_character(str, size, true_size, &true_fsize, v, flags, length, width);
			}
			break;

		case 'f' | LENGTH_INT:
		case 'F' | LENGTH_INT:
		case 'e' | LENGTH_INT:
		case 'E' | LENGTH_INT:
			{
				double v = va_arg(ap, double);
				fsize = format_double(str, size, true_size, &true_fsize, v, flags, precision, length, width, *lpsz);
			}
			break;

		case 'f' | LENGTH_LDOUBLE:
		case 'F' | LENGTH_LDOUBLE:
		case 'e' | LENGTH_LDOUBLE:
		case 'E' | LENGTH_LDOUBLE:
			{
				long double v = va_arg(ap, long double);
				fsize = format_double(str, size, true_size, &true_fsize, v, flags, precision, length, width, *lpsz);
			}
			break;

		case 'g':
		case 'G':
		case 'a':
		case 'A':
		default:
			assert(0);
			break;
		}

		total_size	+= fsize;
		true_size	+= true_fsize;
	}

	output_chr(str, size, true_size, '\0');

	va_end(ap_save);

	return total_size;
}


/**
 *	Output a character & do buffer overflow check.
 *
 *	@param[out]	str		: the buffer.
 *	@param[in]	size	: buffer size.
 *	@param[in]	pos		: position to output the character.
 *	@param[in]	chr		: the character to output.
 *
 *	@note	It will always preserve space for a null terminator in [str] before
 *			writing data to it.
 *
 *	@return		Number of character actually wrote to the output buffer.
 */
static int output_chr(XCHAR* str, size_t size, size_t pos, XCHAR chr)
{
	int retval = 0;

	if ( (NULL != str) )
	{
		if ( ('\0' != chr) && (pos + 1 < size) )
		{
			str[pos]	= chr;
			retval		= 1;
		}
		else if ( ('\0' == chr) && (pos < size) )
		{
			str[pos] = chr;
		}
	}

	return retval;
}


/**
 *	Format a integer parameter.
 *
 *	@param[out]	str			: the buffer to output.
 *	@param[in]	size		: size of the buffer to output.
 *	@param[in]	pos			: position to write formated string in the buffer.
 *	@param[out]	true_size	: number of characters actually wrote to the buffer.
 *	@param[in]	value		: the number to be formated.
 *	@param[in]	is_neg		: whether the number is a negative one.
 *	@param[in]	flags		: fprintf flags.
 *	@param[in]	precision	: value precision when formating the value.
 *	@param[in]	width		: minimum width of the integer field.
 *	@param[in]	base		:
 *	@param[in]	signed_int	: whether the value is signed.
 *	@param[in]	capital		: whether use capital character to format hex value.
 *
 *	@return		Number of characters should have been wrote if the buffer is
 *				sufficiently large.
 */
static size_t format_int(
	XCHAR		*	str,
	size_t			size,
	size_t			pos,
	size_t		*	true_size,
	ddns_uintmax	value,
	int				is_neg,
	int				flags,
	int				precision,
	int				width,
	int				base,
	int				capital )
{
	size_t	field_size	= 0;

	int		sign_len	= 0; /* length of number sign mark (or prefix space)  */
	int		num_len		= 0; /* length of the number exclude sign mark        */
	int		totallen	= 0; /* total length of the field (including paddings)*/

	int		num_pos		= 0;

	ddns_uintmax	tmp_value	= value;

	(*true_size) = 0;

	if ( base <= 1 )
	{
		return field_size;
	}

	/**
	 *	For d, i, o, u, x, and X conversions, if a precision is specified, the
	 *	'0' flag is ignored.
	 */
	if ( precision >= 0 )
	{
		flags &= (~FLAG_PREFIX0);
	}

	/**
	 *	Step 1.1: calculate number length
	 */
	if ( is_neg )
	{
		sign_len = 1;
	}
	else
	{
		if ( (0 != (flags & FLAG_SIGNCONV)) || (0 != (flags & FLAG_SPACE)) )
		{
			sign_len = 1;
		}
	}
	if ( 0 != (flags & FLAG_ALTFORM) )
	{
		switch(base)
		{
		case 8:
			sign_len += 1;
			break;

		case 10:
			break;

		case 16:
			sign_len += 2;
			break;

		default:
			assert(0);
			break;
		}
	}
	for( tmp_value = value; tmp_value > 0; tmp_value /= base )
	{
		++num_len;
	}

	/**
	 *	Step 1.2: calculate number length, considering precision
	 */
	if ( precision >= 0 )
	{
		if ( num_len < precision )
		{
			num_len = precision;
		}
	}
	else
	{
		/* The default precision is 1. */
		if ( num_len < 1 )
		{
			num_len = 1;
		}
	}

	/**
	 *	Step 1.3: calculate total length, considering field width
	 */
	if ( (width >= 0) && (num_len + sign_len < width) )
	{
		totallen = width;
	}
	else
	{
		totallen = num_len;
	}

	/**
	 *	Step 2.1: output sign mark before prefix '0'.
	 */
	if ( (0 == (flags & FLAG_LEFTJUSTIFY)) && (0 != (flags & FLAG_PREFIX0)) )
	{
		size_t mark_size	= 0;
		size_t mark_tsize	= 0;

		mark_size = format_int_prefix(str, size, pos, &mark_tsize, is_neg, flags, base, capital);

		field_size += mark_size;
		(*true_size) += mark_tsize;

		pos += mark_tsize;
	}

	/**
	 * Step 2.2: output prefix paddings.
	 */
	if ( 0 == (flags & FLAG_LEFTJUSTIFY) && (totallen > num_len + sign_len) )
	{
		/* right justify, add paddings before number */

		int prefix_cnt	= totallen - num_len - sign_len;
		XCHAR chr		= ' ';

		if ( 0 != (flags & FLAG_PREFIX0) )
		{
			chr = '0';
		}

		for ( ; prefix_cnt > 0; --prefix_cnt )
		{
			++field_size;
			(*true_size) += output_chr(str, size, pos, chr);
			++pos;
		}
	}

	/**
	 *	Step 2.3: output sign mark after non-'0' prefix.
	 */
	if ( (0 != (flags & FLAG_LEFTJUSTIFY)) || (0 == (flags & FLAG_PREFIX0)) )
	{
		size_t mark_size	= 0;
		size_t mark_tsize	= 0;

		mark_size = format_int_prefix(str, size, pos, &mark_tsize, is_neg, flags, base, capital);

		field_size += mark_size;
		(*true_size) += mark_tsize;

		pos += mark_tsize;
	}

	/**
	 * Step 2.4: output number.
	 */
	tmp_value = value;
	for ( num_pos = num_len; num_pos > 0; --num_pos, tmp_value /= base )
	{
		XCHAR chr = '0';
		if ( (tmp_value % base) >= 10 )
		{
			if ( 0 != capital )
			{
				chr = (XCHAR)('A' + (tmp_value % base) - 10);
			}
			else
			{
				chr = (XCHAR)('a' + (tmp_value % base) - 10);
			}
		}
		else
		{
			chr = (XCHAR)('0' + (tmp_value % base));
		}

		++field_size;
		(*true_size) += output_chr(str, size, pos + num_pos - 1, chr);
	}
	pos += num_len;

	/**
	 * Step 2.5: output suffix paddings.
	 */
	if ( 0 != (flags & FLAG_LEFTJUSTIFY) && (totallen > num_len + sign_len) )
	{
		/* left justify, add paddings after number */
		int suffix_cnt	= totallen - num_len - sign_len;
		for ( ; suffix_cnt > 0; --suffix_cnt )
		{
			++field_size;
			(*true_size) += output_chr(str, size, pos, ' ');
			++pos;
		}
	}

	return field_size;
}


/**
 *	Output sign mark and 'alternative form' prefix.
 *
 *	@param[out]	str			: the buffer to output.
 *	@param[in]	size		: size of the buffer to output.
 *	@param[in]	pos			: position to write formated string in the buffer.
 *	@param[out]	true_size	: number of characters actually wrote to the buffer.
 *	@param[in]	is_negative	: whether output '-' in the buffer.
 *	@param[in]	flags		: fprintf flags.
 *	@param[in]	base		:
 *	@param[in]	capital		: whether use capital character.
 *
 *	@return		Number of characters should have been wrote to the buffer if
 *				it's sufficiently large.
 */
static int format_int_prefix(
	XCHAR	*	str,
	size_t		size,
	size_t		pos,
	size_t	*	true_size,
	int			is_negative,
	int			flags,
	int			base,
	int			capital )
{
	int field_size = 0;

	(*true_size) = 0;

	/* Step 1: handle sign mark */
	if ( 0 != is_negative )
	{
		assert( 10 == base );

		++field_size;
		(*true_size) += output_chr(str, size, pos, '-');
	}
	else
	{
		/* If the space and + flags both appear, the space flag is ignored. */
		if ( 0 != (flags & FLAG_SIGNCONV) )
		{
			++field_size;
			(*true_size) += output_chr(str, size, pos, '+');
		}
		else if ( 0 != (flags & FLAG_SPACE) )
		{
			++field_size;
			(*true_size) += output_chr(str, size, pos, ' ');
		}
		else
		{
			/* do nothing */
		}
	}

	/* Step 2: handle 'alternative form' prefix */
	if ( 0 != (flags & FLAG_ALTFORM) )
	{
		switch( base )
		{
		case 8:
			++field_size;
			(*true_size) += output_chr(str, size, pos, '0');
			break;

		case 10:
			break;

		case 16:
			++field_size;
			(*true_size) += output_chr(str, size, pos, '0');
			if ( 0 != capital )
			{
				++field_size;
				(*true_size) += output_chr(str, size, pos + 1, 'X');
			}
			else
			{
				++field_size;
				(*true_size) += output_chr(str, size, pos + 1, 'x');
			}
			break;
		}
	}

	return field_size;
}


/**
 *	Format a string parameter.
 *
 *	@param[out]	str			: the buffer to output.
 *	@param[in]	size		: size of the buffer to output.
 *	@param[in]	pos			: position to write formated string in the buffer.
 *	@param[out]	true_size	: number of characters actually wrote to the buffer.
 *	@param[in]	value		: the string to be formated.
 *	@param[in]	flags		: fprintf flags.
 *	@param[in]	precision	: value precision when formating the value.
 *	@param[in]	length		: 'length modifier' of the argument.
 *	@param[in]	width		: minimum width of the integer field.
 *
 *	@return		Number of characters should have been wrote if the buffer is
 *				sufficiently large.
 */
static size_t format_string(
	XCHAR	*		str,
	size_t			size,
	size_t			pos,
	size_t	*		true_size,
	const XCHAR	*	value,
	int				flags,
	int				precision,
	int				length,
	int				width
	)
{
	size_t			field_size		= 0;
	int				idx				= 0;
	int				padding_size	= 0;
	XCHAR			padding_chr		= ' ';
	int				s_len			= 0;

	if ( 0 != (flags & LENGTH_LONG) )
	{
		/* wide characters are not supported yet */
		assert(0);
	}

	if ( NULL == value )
	{
		value = "(null)";
	}

	(*true_size) = 0;

	s_len = strlen(value);
	if ( (precision >= 0) && (s_len > precision) )
	{
		s_len = precision;
	}
	if ( width > s_len )
	{
		padding_size = width - s_len;
	}

	/* Step 1: prefix paddings if necessary. */
	if ( (padding_size > 0) && (0 == (flags & FLAG_LEFTJUSTIFY)) )
	{
		for( ; padding_size > 0; --padding_size )
		{
			++field_size;
			(*true_size) += output_chr(str, size, pos, padding_chr);
			++pos;
		}
	}

	/* Step 2: output the string */
	for ( idx = 0 ; idx < s_len; ++idx, ++value )
	{
		++field_size;
		(*true_size) += output_chr(str, size, pos, *value);
		++pos;
	}

	/* Step 3: suffix paddings if necessary. */
	if ( (padding_size > 0) && (0 != (flags & FLAG_LEFTJUSTIFY)) )
	{
		for( ; padding_size > 0; --padding_size )
		{
			++field_size;
			(*true_size) += output_chr(str, size, pos, padding_chr);
			++pos;
		}
	}

	return field_size;
}


/**
 *	Format a string parameter.
 *
 *	@param[out]	str			: the buffer to output.
 *	@param[in]	size		: size of the buffer to output.
 *	@param[in]	pos			: position to write formated string in the buffer.
 *	@param[out]	true_size	: number of characters actually wrote to the buffer.
 *	@param[out]	field_size	: number of characters should have been wrote if the
 *							  buffer is sufficiently large.
 *	@param[in]	ap			: the parameter list.
 *	@param[in]	flags		: fprintf flags.
 *	@param[in]	length		: 'length modifier' of the argument.
 *	@param[in]	width		: minimum width of the integer field.
 *
 *	@return		required buffer size to format the value, not including null
 *				terminator.
 */
static size_t format_character(
	XCHAR	*	str,
	size_t		size,
	size_t		pos,
	size_t	*	true_size,
	XCHAR		value,
	int			flags,
	int			length,
	int			width
	)
{
	size_t			field_size		= 0;
	int				padding_size	= 0;
	XCHAR			padding_chr		= ' ';

	(*true_size) = 0;

	if ( width > 1 )
	{
		padding_size = width - 1;
	}

	(*true_size) = 0;

	/* Step 1: prefix paddings if necessary. */
	if ( (width > 1) && (0 == (flags & FLAG_LEFTJUSTIFY)) )
	{
		for( ; padding_size > 0; --padding_size )
		{
			++field_size;
			(*true_size) += output_chr(str, size, pos, padding_chr);
			++pos;
		}
	}

	/* Step 2: output the character */
	++field_size;
	(*true_size) += output_chr(str, size, pos, value);
	++pos;

	/* Step 3: suffix paddings if necessary. */
	if ( (padding_size > 0) && (0 != (flags & FLAG_LEFTJUSTIFY)) )
	{
		for( ; padding_size > 0; --padding_size )
		{
			++field_size;
			(*true_size) += output_chr(str, size, pos, padding_chr);
			++pos;
		}
	}

	return field_size;
}


/**
 *	Format a integer parameter.
 *
 *	@param[out]	str			: the buffer to output.
 *	@param[in]	size		: size of the buffer to output.
 *	@param[in]	pos			: position to write formated string in the buffer.
 *	@param[out]	true_size	: number of characters actually wrote to the buffer.
 *	@param[in]	value		: the value to be formated.
 *	@param[in]	flags		: fprintf flags.
 *	@param[in]	precision	: value precision when formating the value.
 *	@param[in]	length		: 'length modifier' of the argument.
 *	@param[in]	width		: minimum width of the integer field.
 *	@param[in]	format		: the conversion specifier.
 *
 *	@return		Number of characters should have been wrote if the buffer is
 *				sufficiently large.
 */
static size_t format_double(
	XCHAR		*	str,
	size_t			size,
	size_t			pos,
	size_t		*	true_size,
	long double		value,
	int				flags,
	int				precision,
	int				length,
	int				width,
	XCHAR			format
	)
{
	size_t		field_size	= 0;
	int			idx			= 0;
	int			exponential	= 0;	/* use only if format is 'e' or 'E' */
	int			p_len		= 0;	/* length of padding                */
	int			s_len		= 0;	/* length of sign mark              */
	int			i_len		= 0;	/* length before decimal point      */
	int			d_len		= 0;	/* length of decimal point          */

	(*true_size) = 0;

	/**
	 *	Step 1: calculate number length.
	 */
	if ( (value < 0) || (0 != (flags & FLAG_SIGNCONV)) || (0 != (flags & FLAG_SPACE)) )
	{
		s_len = 1;
	}
	if ( ('f' == format) || ('F' == format) )
	{
		if ( value >= 10.0f )
		{
			i_len = (int)floor(log10(value)) + 1;
		}
		else if ( value > -10.0f )
		{
			i_len = 1;
		}
		else
		{
			i_len = (int)floor(log10(-value)) + 1;
		}
	}
	else if ( ('e' == format) || ('E' == format) )
	{
		i_len = 1;
		if ( value > 0.0f )
		{
			exponential = (int)floor(log10(value));
		}
		else if ( value < 0.0f )
		{
			exponential = (int)floor(log10(-value));
		}
		value /= pow(10.0f, exponential);
	}
	else
	{
		assert(0);
	}
	if ( (0 != precision) || (0 != (flags & FLAG_ALTFORM)) )
	{
		/**
		 *	For a, A, e, E, f, F, g, and G conversions, the result of converting
		 *	a floating-point number always contains a decimal-point character,
		 *	even if no digits follow it. (Normally, a decimal-point character
		 *	appears in the result of these conversions only if a digit follows
		 *	it.)
		 */
		d_len = 1;
	}
	if( precision < 0 )
	{
		/* If the precision is missing, it is taken as 6. */
		precision = 6;
	}
	if ( width > (s_len + i_len + d_len + precision) )
	{
		p_len = width - (s_len + i_len + d_len + precision);
	}

	/**
	 *	Step 2: Do rounding before formating the floating-point number.
	 */
	if ( value >= 0 )
	{
		value = floor(value * pow(10.0f, (double)precision) + 0.5f);
	}
	else
	{
		value = ceil(value * pow(10.0f, (double)precision) - 0.5f);
	}


	/**
	 *	Step 3.1: output sign mark before prefix '0'.
	 */
	if ( (0 == (flags & FLAG_LEFTJUSTIFY)) && (0 != (flags & FLAG_PREFIX0)) )
	{
		size_t	mark_size	= 0;
		size_t	mark_tsize	= 0;
		int		is_neg		= (value < 0 ? 1 : 0);

		mark_size = format_int_prefix(str, size, pos, &mark_tsize, is_neg, flags, 10, 0);

		field_size += mark_size;
		(*true_size) += mark_tsize;

		pos += mark_tsize;
	}

	/**
	 * Step 3.2: output prefix paddings.
	 */
	if ( 0 == (flags & FLAG_LEFTJUSTIFY) && (p_len > 0) )
	{
		/* right justify, add paddings before number */
		XCHAR chr		= ' ';
		if ( 0 != (flags & FLAG_PREFIX0) )
		{
			chr = '0';
		}

		for ( ; p_len > 0; --p_len )
		{
			++field_size;
			(*true_size) += output_chr(str, size, pos, chr);
			++pos;
		}
	}

	/**
	 *	Step 3.3: output sign mark after non-'0' prefix.
	 */
	if ( (0 != (flags & FLAG_LEFTJUSTIFY)) || (0 == (flags & FLAG_PREFIX0)) )
	{
		size_t	mark_size	= 0;
		size_t	mark_tsize	= 0;
		int		is_neg		= (value < 0 ? 1 : 0);

		mark_size = format_int_prefix(str, size, pos, &mark_tsize, is_neg, flags, 10, 0);

		field_size += mark_size;
		(*true_size) += mark_tsize;

		pos += mark_tsize;
	}

	/**
	 *	Step 3.4: output floating point number
	 */
	if ( value < 0 )
	{
		value = -value;
	}
	for ( idx = precision; idx > 0; --idx, value /= 10 )
	{
		int mod = (int)fmod(value, 10.0f);
		++field_size;
		(*true_size) += output_chr(str, size, pos + i_len + d_len + idx - 1, (XCHAR)('0' + mod));
	}
	if ( d_len > 0 )
	{
		++field_size;
		(*true_size) += output_chr(str, size, pos + i_len, '.');
	}
	for ( idx = i_len; idx > 0; --idx, value /= 10 )
	{
		int mod = (int)fmod(value, 10.0f);
		++field_size;
		(*true_size) += output_chr(str, size, pos + idx - 1, (XCHAR)('0' + mod));
	}
	pos += (i_len + d_len + precision);

	/**
	 * Step 3.5: output suffix paddings.
	 */
	if ( 0 != (flags & FLAG_LEFTJUSTIFY) && (p_len > 0) )
	{
		/* left justify, add paddings after number */
		for ( ; p_len > 0; --p_len )
		{
			++field_size;
			(*true_size) += output_chr(str, size, pos, ' ');
			++pos;
		}
	}

	if ( ('e' == format) || ('E' == format) )
	{
		size_t			e_size	= 0;
		size_t			e_tsize	= 0;
		int				is_neg	= exponential < 0 ? 1 : 0;
		ddns_uintmax	value	= exponential < 0 ? -exponential : exponential;

		++field_size;
		(*true_size) += output_chr(str, size, pos, format);
		++pos;

		e_size = format_int(str, size, pos, &e_tsize, value, is_neg, FLAG_SIGNCONV | FLAG_PREFIX0, -1, 3, 10, 0);

		pos				+= e_size;
		field_size		+= e_size;
		(*true_size)	+= e_tsize;
	}

	return field_size;
}
