/*
 *	Copyright (C) 2009-2010 K.R.F. Studio.
 *
 *	$Id: json.c 260 2010-12-26 09:10:26Z karl $
 *
 *	This file is part of 'ddns', Created by karl on 2009-05-31.
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
 *	Provide json format serialization support.
 */

#include "json.h"
#include <stdio.h>		/* printf         */
#include <stdlib.h>		/* malloc, strtol */
#include <string.h>		/* memset         */
#include <assert.h>		/* assert         */
#include <math.h>		/* pow            */


/****************************************************************************/
/* json basic type definition                                               */
/****************************************************************************/

/* json string */
typedef struct _json_string
{
	unsigned long		size;
	char				*value;
}									json_string;

/* json number */
typedef struct _json_number
{
	double				value;
}									json_number;

/* json name-value pair */
typedef struct _json_pair
{
	struct json_value	*name;
	struct json_value	*value;
	struct _json_pair	*next;
}									json_pair;

/* json object */
typedef struct _json_object
{
	json_pair			*pairs;
}									json_object;

/* json array */
typedef struct _json_array
{
	unsigned long		size;
	struct json_value	**value;
}									json_array;

/* json value element */
struct json_value
{
	enum json_value_type	type;
	union
	{
		json_string			string;
		json_number			number;
		json_object			object;
		json_array			array;
	}						value;
};

enum json_classes {
	_____	= -1,	/* invalid */
    C_SPACE	= 0,	/* space */
    C_WHITE,		/* other whitespace */
    C_LCURB,		/* {  */
    C_RCURB,		/* } */
    C_LSQRB,		/* [ */
    C_RSQRB,		/* ] */
    C_COLON,		/* : */
    C_COMMA,		/* , */
    C_QUOTE,		/* " */
    C_BACKS,		/* \ */
    C_SLASH,		/* / */
    C_PLUS, 		/* + */
    C_MINUS,		/* - */
    C_POINT,		/* . */
    C_ZERO,			/* 0 */
    C_DIGIT,		/* 123456789 */
    C_LOW_A,		/* a */
    C_LOW_B,		/* b */
    C_LOW_C,		/* c */
    C_LOW_D,		/* d */
    C_LOW_E,		/* e */
    C_LOW_F,		/* f */
    C_LOW_L,		/* l */
    C_LOW_N,		/* n */
    C_LOW_R,		/* r */
    C_LOW_S,		/* s */
    C_LOW_T,		/* t */
    C_LOW_U,		/* u */
    C_ABCDF,		/* ABCDF */
    C_E,			/* E */
    C_ETC,			/* everything else */
    NR_CLASSES
};

enum json_states
{
    GO,  /* start    */
    OK,  /* ok       */
    OB,  /* object   */
    KE,  /* key      */
    CO,  /* colon    */
    VA,  /* value    */
    AR,  /* array    */
    ST,  /* string   */
    ES,  /* escape   */
    U1,  /* u1       */
    U2,  /* u2       */
    U3,  /* u3       */
    U4,  /* u4       */
    MI,  /* minus    */
    ZE,  /* zero     */
    IN,  /* integer  */
    FR,  /* fraction */
    E1,  /* e        */
    E2,  /* ex       */
    E3,  /* exp      */
    T1,  /* tr       */
    T2,  /* tru      */
    T3,  /* true     */
    F1,  /* fa       */
    F2,  /* fal      */
    F3,  /* fals     */
    F4,  /* false    */
    N1,  /* nu       */
    N2,  /* nul      */
    N3,  /* null     */
    NR_STATES,
	__ = -1,  /* invalid           */
	_C = -2,  /* colon             */
	_V = -3,  /* comma, end of key */
	_S = -4,  /* end of string     */
	_A = -5,  /* start of array    */
	_O = -6,  /* start of object   */
	_Y = -7,  /* end of array      */
	_T = -8   /* end of object     */
};

enum json_modes
{
	MODE_DONE,
	MODE_VALUE,
	MODE_ARRAY,
	MODE_OBJECT,
	MODE_KEY
};

struct json_parse_value
{
	struct	json_value			*	object;		/* the value being parsed    */

	/*-*- the following items are used when parsing object/array element  -*-*/
	struct	json_parse_value	*	parent;		/* parent object             */
	/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

	/*-*-*-*-*-*- the following items are used when parsing numbers -*-*-*-*-*/
	int								negative;	/* negative number flag      */
	int								pow;		/* exp number                */
	/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

	/*-*-*- the following items are used when parsing unicode character -*-*-*/
	int								chr;		/* unicode scalar value      */
	/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

	/*-*-*-*-*-*- the following items are used when parsing object  -*-*-*-*-*/
	struct	json_value			*	key;		/* name of the key pair      */
	struct	json_value			*	value;		/* value of the key pair     */
	/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
};

struct json_context
{
	int								stack_top;	/* mode stack pointer        */
	int								stack_size;	/* depth of mode stack       */
	enum	json_states				state;		/* current parsing state     */
	enum	json_modes			*	mode_stack;	/* parsing mode stack        */
	struct	json_parse_value	*	target;		/* the value being parsed    */
};

/*
    This array maps the 128 ASCII characters into character classes.
    The remaining Unicode characters should be mapped to C_ETC.
    Non-whitespace control characters are errors.
*/
static enum json_classes json_ascii_class[128] =
{
    _____,   _____,   _____,   _____,   _____,   _____,   _____,   _____,
    _____,   C_WHITE, C_WHITE, _____,   _____,   C_WHITE, _____,   _____,
    _____,   _____,   _____,   _____,   _____,   _____,   _____,   _____,
    _____,   _____,   _____,   _____,   _____,   _____,   _____,   _____,

    C_SPACE, C_ETC,   C_QUOTE, C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
    C_ETC,   C_ETC,   C_ETC,   C_PLUS,  C_COMMA, C_MINUS, C_POINT, C_SLASH,
    C_ZERO,  C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT,
    C_DIGIT, C_DIGIT, C_COLON, C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,

    C_ETC,   C_ABCDF, C_ABCDF, C_ABCDF, C_ABCDF, C_E,     C_ABCDF, C_ETC,
    C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
    C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
    C_ETC,   C_ETC,   C_ETC,   C_LSQRB, C_BACKS, C_RSQRB, C_ETC,   C_ETC,

    C_ETC,   C_LOW_A, C_LOW_B, C_LOW_C, C_LOW_D, C_LOW_E, C_LOW_F, C_ETC,
    C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_LOW_L, C_ETC,   C_LOW_N, C_ETC,
    C_ETC,   C_ETC,   C_LOW_R, C_LOW_S, C_LOW_T, C_LOW_U, C_ETC,   C_ETC,
    C_ETC,   C_ETC,   C_ETC,   C_LCURB, C_ETC,   C_RCURB, C_ETC,   C_ETC
};


/**
 *  The state transition table takes the current state and the current symbol,
 *  and returns either a new state or an action. An action is represented as a
 *  negative number. A JSON text is accepted if at the end of the text the
 *  state is OK and if the mode is MODE_DONE.
 */
static enum json_states json_state_table[NR_STATES][NR_CLASSES] = {
/*               white                                      1-9                                   ABCDF  etc
             space |  {  }  [  ]  :  ,  "  \  /  +  -  .  0  |  a  b  c  d  e  f  l  n  r  s  t  u  |  E  |*/
/*start  GO*/ {GO,GO,_O,__,_A,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*ok     OK*/ {OK,OK,__,_T,__,_Y,__,_V,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*object OB*/ {OB,OB,__,_T,__,__,__,__,ST,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*key    KE*/ {KE,KE,__,__,__,__,__,__,ST,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*colon  CO*/ {CO,CO,__,__,__,__,_C,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*value  VA*/ {VA,VA,_O,__,_A,__,__,__,ST,__,__,__,MI,__,ZE,IN,__,__,__,__,__,F1,__,N1,__,__,T1,__,__,__,__},
/*array  AR*/ {AR,AR,_O,__,_A,_Y,__,__,ST,__,__,__,MI,__,ZE,IN,__,__,__,__,__,F1,__,N1,__,__,T1,__,__,__,__},
/*string ST*/ {ST,__,ST,ST,ST,ST,ST,ST,_S,ES,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST},
/*escape ES*/ {__,__,__,__,__,__,__,__,ST,ST,ST,__,__,__,__,__,__,ST,__,__,__,ST,__,ST,ST,__,ST,U1,__,__,__},
/*u1     U1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,U2,U2,U2,U2,U2,U2,U2,U2,__,__,__,__,__,__,U2,U2,__},
/*u2     U2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,U3,U3,U3,U3,U3,U3,U3,U3,__,__,__,__,__,__,U3,U3,__},
/*u3     U3*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,U4,U4,U4,U4,U4,U4,U4,U4,__,__,__,__,__,__,U4,U4,__},
/*u4     U4*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,ST,ST,ST,ST,ST,ST,ST,ST,__,__,__,__,__,__,ST,ST,__},
/*minus  MI*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,ZE,IN,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*zero   ZE*/ {OK,OK,__,_T,__,_Y,__,_V,__,__,__,__,__,FR,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*int    IN*/ {OK,OK,__,_T,__,_Y,__,_V,__,__,__,__,__,FR,IN,IN,__,__,__,__,E1,__,__,__,__,__,__,__,__,E1,__},
/*frac   FR*/ {OK,OK,__,_T,__,_Y,__,_V,__,__,__,__,__,__,FR,FR,__,__,__,__,E1,__,__,__,__,__,__,__,__,E1,__},
/*e      E1*/ {__,__,__,__,__,__,__,__,__,__,__,E2,E2,__,E3,E3,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*ex     E2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,E3,E3,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*exp    E3*/ {OK,OK,__,_T,__,_Y,__,_V,__,__,__,__,__,__,E3,E3,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*tr     T1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,T2,__,__,__,__,__,__},
/*tru    T2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,T3,__,__,__},
/*true   T3*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,OK,__,__,__,__,__,__,__,__,__,__},
/*fa     F1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,F2,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*fal    F2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,F3,__,__,__,__,__,__,__,__},
/*fals   F3*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,F4,__,__,__,__,__},
/*false  F4*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,OK,__,__,__,__,__,__,__,__,__,__},
/*nu     N1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,N2,__,__,__},
/*nul    N2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,N3,__,__,__,__,__,__,__,__},
/*null   N3*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,OK,__,__,__,__,__,__,__,__},
};


/****************************************************************************/
/* local functions                                                          */
/****************************************************************************/

/**
 *	Push json mode.
 *
 *	@param[in]	ctx		: pointer to the json context object.
 *	@param[in]	mode	: the json mode to be pushed.
 *
 *	@return		Return non-zero if succeeded, otherwise 0 is returned.
 */
static int json_push(struct json_context * ctx, enum json_modes mode)
{
	if ( (NULL == ctx) || (ctx->stack_top >= ctx->stack_size) )
	{
		return 0;
	}

	ctx->mode_stack[++(ctx->stack_top)] = mode;

	return 1;
}


/**
 *	Pop json mode.
 *
 *	@param[in]	ctx		: pointer to the json context object.
 *	@param[in]	mode	: the json mode that is expected on the stack top.
 *
 *	@return		Return non-zero if succeeded, otherwise 0 is returned.
 */
static int json_pop(struct json_context * ctx, enum json_modes mode)
{
	if ( (NULL == ctx) || (0 == ctx->stack_top) )
	{
		if ( (MODE_DONE == mode) && (mode == ctx->mode_stack[ctx->stack_top]) )
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	if ( mode != ctx->mode_stack[ctx->stack_top] )
	{
		return 0;
	}

	--(ctx->stack_top);

	return 1;
}


/****************************************************************************/
/* implementation of functions                                              */
/****************************************************************************/

/**
 *	Create a [json_value] object. Default values for different type:
 *
 *		json_type_null		: null
 *		json_type_string	: ""
 *		json_type_number	: 0
 *		json_type_object	: {}
 *		json_type_array		: []
 *		json_type_true		: true
 *		json_type_false		: false
 *
 *	@return		Return pointer to the newly created json_value object. When you
 *				don't need it, use [json_destroy] to free the object.
 */
struct json_value * json_create(enum json_value_type type)
{
	struct json_value * value =
		(struct json_value *)malloc(sizeof(struct json_value));

	memset(value, 0, sizeof(*value));
	value->type = type;

	return value;
}


/**
 *	Destroy the [json_vlaue] object and free resources allocated for the
 *	[json_value] object.
 *
 *	@param[in]	var		: the [json_value] object to be destroyed.
 */
void json_destroy(struct json_value * var)
{
	if ( NULL != var )
	{
		unsigned long i = 0;
		switch ( var->type )
		{
		case json_type_null:
		case json_type_number:
		case json_type_true:
		case json_type_false:
			/* do nothing */
			break;

		case json_type_string:
			if ( NULL != var->value.string.value )
			{
				free(var->value.string.value);
			}
			break;

		case json_type_object:
			while ( NULL != var->value.object.pairs )
			{
				json_pair * pair = var->value.object.pairs;

				json_destroy(pair->name);
				json_destroy(pair->value);

				var->value.object.pairs = pair->next;
				free(pair);
			}
			break;

		case json_type_array:
			for ( i = 0; i < var->value.array.size; ++i )
			{
				json_destroy(var->value.array.value[i]);
			}
			free(var->value.array.value);
			break;

		default:
			assert(0);
			break;
		}

		free(var);
	}
}


/**
 *	Create a copy of a [json_value] object.
 *
 *	@param[in]	var		: the object to be copied.
 *
 *	@return		Return pointer to the newly created [json_value] object. If the
 *				copy failed, NULL will be returned. When it's no longer needed,
 *				use [json_destroy] to free it.
 */
struct json_value * json_duplicate(struct json_value * var)
{
	json_pair * pairs = NULL;
	struct json_value * new_var = NULL;
	enum json_value_type type = json_get_type(var);

	if ( json_type_invalid != type )
	{
		new_var = json_create(type);
	}

	if ( NULL != new_var )
	{
		switch ( type )
		{
		case json_type_string:
			if ( -1 == json_string_set(new_var, var->value.string.value) )
			{
				json_destroy(new_var);
				new_var = NULL;
			}
			break;

		case json_type_number:
			new_var->value.number.value = var->value.number.value;
			break;

		case json_type_object:
			for (	pairs = var->value.object.pairs;
					NULL != pairs;
					pairs = pairs->next )
			{
				if ( -1 == json_object_set(new_var, pairs->name->value.string.value, pairs->value) )
				{
					json_destroy(new_var);
					new_var = NULL;
					break;
				}
			}
			break;

		case json_type_array:
			new_var->value.array.value =
				malloc(var->value.array.size * sizeof(var->value.array.value));
			if ( NULL != new_var->value.array.value )
			{
				unsigned long i = 0;
				for ( i = 0; i < var->value.array.size; ++i )
				{
					new_var->value.array.value[i] =
						json_duplicate(var->value.array.value[i]);
					if ( NULL == new_var->value.array.value[i] )
					{
						json_destroy(new_var);
						new_var = NULL;
						break;
					}

					new_var->value.array.size = i + 1;
				}
			}
			break;

		case json_type_true:
		case json_type_false:
		case json_type_null:
			/* do nothing */
			break;

		default:
			assert(0);
			json_destroy(new_var);
			new_var = NULL;
			break;
		}
	}

	return new_var;
}


/**
 *	Get type of a [json_value] object.
 *
 *	@param[in]	var		: pointer to the [json_value] object.
 *
 *	@return		Return type of the [json_value] object. If invalid object is
 *				passed in, json_type_invalid will be returned.
 */
enum json_value_type json_get_type(struct json_value * var)
{
	enum json_value_type type = json_type_invalid;
	if ( NULL != var )
	{
		type = var->type;
	}

	return type;
}


/**
 *	Get value of a json string object.
 *
 *	@param[in]	var		: the [json_value] object of type [json_type_string].
 *
 *	@return		Return value of the [json_value] object. If [var] is not a
 *				string, NULL will be returned.
 */
const char * json_string_get(struct json_value * var)
{
	const char * ret = NULL;
	if ( json_type_string == json_get_type(var) )
	{
		ret = var->value.string.value;
	}

	return ret;
}


/**
 *	Modify value of a json string object.
 *
 *	@param[out]	var		: the [json_value] object of type [json_type_string].
 *	@param[in]	string	: the value to be set to [var].
 *
 *	@note		If [var] is not of type [json_type_string], the function fails.
 *
 *	@return		Return 0 if the operation succeeded. Otherwise -1 is returned.
 */
int json_string_set(struct json_value * var, char * string)
{
	int result = -1;
	if ( json_type_string == json_get_type(var) )
	{
		unsigned long length = NULL == string ? 0 : strlen(string);
		if ( length < var->value.string.size )
		{
			result = 0;
		}
		else
		{
			/* at least 16 bytes is allocated */
			unsigned long size = ((length + 16) & 0xFFFFFFF0);
			char * new_buf = realloc(var->value.string.value, size);
			if ( NULL != new_buf )
			{
				result = 0;

				var->value.string.size = size;
				var->value.string.value = new_buf;
			}
		}

		if ( 0 == result )
		{
			var->value.string.value[length] = '\0';
			memcpy(var->value.string.value, string, length);
		}
	}

	return result;
}


/**
 *	Get value of a json number object.
 *
 *	@param[in]	var		: the [json_value] object of type [json_type_number].
 *
 *	@return		Return value of the [json_value] object. If [var] is not a
 *				number, 0 will be returned.
 */
double json_number_get(struct json_value * var)
{
	double ret = 0.0;
	if ( json_type_number == json_get_type(var) )
	{
		ret = var->value.number.value;
	}

	return ret;
}


/**
 *	Modify value of a json number object.
 *
 *	@param[out]	var		: the [json_value] object of type [json_type_number].
 *	@param[in]	string	: the value to be set to [var].
 *
 *	@note		If [var] is not of type [json_type_number], the function fails.
 *
 *	@return		Return 0 if the operation succeeded. Otherwise -1 is returned.
 */
int json_number_set(struct json_value * var, int number)
{
	int result = -1;
	if ( json_type_number == json_get_type(var) )
	{
		result = 0;
		var->value.number.value = number;
	}

	return result;
}


/**
 *	Get member value of a json object object.
 *
 *	@param[in]	var		: the [json_value] object of type [json_type_object].
 *	@param[in]	key		: name of the object member.
 *
 *	@return		Return value of the json object's member, if the key is not
 *				found in the object, NULL will be returned. Use [json_destroy]
 *				to free the returned object when it's no longer needed.
 */
struct json_value * json_object_get(struct json_value * var, char * key)
{
	struct json_value * value = NULL;

	if ( (NULL != key) || (json_type_object == json_get_type(var)) )
	{
		json_pair * pairs = var->value.object.pairs;
		for ( ; NULL != pairs; pairs = pairs->next )
		{
			if ( 0 == strcmp(pairs->name->value.string.value, key) )
			{
				value = pairs->value;
				break;
			}
		}
	}

	if ( NULL != value )
	{
		value = json_duplicate(value);
	}

	return value;
}


/**
 *	Set member value of a json object object.
 *
 *	@param[in]	var		: the [json_value] object of type [json_type_object].
 *	@param[in]	key		: name of the object member.
 *
 *	@return		Return 0 if the operation succeeded. Otherwise -1 is returned.
 */
int json_object_set(
	struct json_value	*	var,
	char				*	key,
	struct json_value	*	value
	)
{
	int result = -1;
	enum json_value_type type = json_get_type(var);

	if ( json_type_object == type )
	{
		json_pair * pairs = var->value.object.pairs;

		for ( ; NULL != pairs; pairs = pairs->next )
		{
			if ( (NULL == key) && (NULL == pairs->name->value.string.value) )
			{
				break;
			}
			else if ( NULL == key )
			{
				/* do nothing */
			}
			else if ( 0 == strcmp(pairs->name->value.string.value, key) )
			{
				break;
			}
		}
		if ( NULL == pairs )
		{
			pairs = malloc(sizeof(*pairs));
			if ( NULL != pairs )
			{
				pairs->name = json_create(json_type_string);
				if ( NULL != pairs->name )
				{
					if ( -1 == json_string_set(pairs->name, key) )
					{
						json_destroy(pairs->name);
						pairs->name = NULL;

						free(pairs);
						pairs = NULL;
					}
				}
				else
				{
					free(pairs);
					pairs = NULL;
				}
			}
			if ( NULL != pairs )
			{
				pairs->value = json_duplicate(value);
				if ( NULL != pairs->value )
				{
					result = 0;

					pairs->next = var->value.object.pairs;
					var->value.object.pairs = pairs;
				}
				else
				{
					json_destroy(pairs->name);
					pairs->name = NULL;

					free(pairs);
					pairs = NULL;
				}
			}
		}
		else
		{
			struct json_value * new_var = json_duplicate(pairs->value);
			if ( NULL != new_var )
			{
				result = 0;

				json_destroy(pairs->value);
				pairs->value = new_var;
			}
		}
	}

	return result;
}


/**
 *	Append a value into an array.
 *
 *	@param[in]	var		: the [json_value] object of type [json_type_array].
 *	@param[in]	value	: the value to be appended.
 *
 *	@return		Return 0 if the operation succeeded. Otherwise -1 is returned.
 */
int json_array_append(
	struct json_value	*	var,
	struct json_value	*	value
	)
{
	static const int RESULT_SUCCESS = 0;
	static const int RESULT_FAILURE = -1;

	int result	= RESULT_SUCCESS;

	if ( json_type_array != json_get_type(var) )
	{
		result = RESULT_FAILURE;
	}
	if ( json_type_invalid == json_get_type(value) )
	{
		result = RESULT_FAILURE;
	}

	if ( RESULT_SUCCESS == result )
	{
		unsigned long			new_size	= var->value.array.size + 1;
		struct json_value	**	array		= NULL;
		struct json_value	*	element		= json_duplicate(value);
		if ( NULL == element )
		{
			result = RESULT_FAILURE;
		}
		else
		{
			array = realloc(var->value.array.value,
							new_size * sizeof(struct json_value*) );
			if ( NULL != array )
			{
				array[new_size - 1]		= element;
				var->value.array.value	= array;
				var->value.array.size	= new_size;
			}
			else
			{
				result = RESULT_FAILURE;
			}
		}
	}

	return result;
}


/**
 *	Modify value of a json string object.
 *
 *	@param[out]	var		: the [json_string] object.
 *	@param[in]	string	: the value to be set to [var].
 *
 *	@note		If [var] is not of type [json_type_string], the function fails.
 *
 *	@return		Return 0 if the operation succeeded. Otherwise -1 is returned.
 */
static int json_string_append(struct json_value * var, char chr)
{
	unsigned long length = 0;
	json_string *s = NULL;

	if ( (NULL == var) || (json_type_string != json_get_type(var)) )
	{
		return -1;
	}

	s = &(var->value.string);

	length = (NULL != s->value) ? strlen(s->value) : 0;
	if ( length + 1 >= var->value.string.size )
	{
		int newlen = (s->size + 1 + 15) & 0xFFFFFFF0;
		char * newval = realloc(s->value, newlen);
		if ( NULL == newval )
		{
			return -1;
		}

		s->size = newlen;
		s->value = newval;
	}

	s->value[length] = chr;
	s->value[length + 1] = '\0';

	return 0;
}


/**
 *	Push a json value into json context.
 *
 *	@param[out]		ctx		- the json parse context.
 *	@param[in]		type	- type of the new value to be pushed.
 *
 *	@return		Return 0 if successful, otherwise return -1.
 */
static int json_push_value(
	struct	json_context		*	ctx,
	enum	json_value_type			type
	)
{
	static const int RESULT_SUCCESS = 0;
	static const int RESULT_FAILURE = -1;

	int								result	= RESULT_SUCCESS;
	struct	json_parse_value	*	value	= NULL;

	if ( NULL == ctx )
	{
		result = RESULT_FAILURE;
	}

	if ( RESULT_SUCCESS == result )
	{
		value = malloc(sizeof(struct json_parse_value));
		if ( NULL != value )
		{
			memset(value, 0, sizeof(*value));
		}
		else
		{
			result = RESULT_FAILURE;
		}
	}

	if ( RESULT_SUCCESS == result )
	{
		value->object = json_create(type);
		if ( NULL == value->object )
		{
			result = RESULT_FAILURE;
		}
	}

	if ( RESULT_SUCCESS == result )
	{
		value->parent = ctx->target;
		ctx->target = value;
	}
	else
	{
		if ( NULL != value )
		{
			if ( NULL != value->object )
			{
				json_destroy(value->object);
				value->object = NULL;
			}

			free(value);
			value = NULL;
		}
	}

	return result;
}


/**
 *	Read a char into the json parse context.
 *
 *	@param[out]		ctx		- the json parse context.
 *	@param[in]		state	- new state.
 *	@param[in]		type	- type of the new value to be pushed.
 *
 *	@return		Return 0 if successful, otherwise return -1.
 */
int json_readchr(struct json_context * ctx, char chr)
{
	int					result		= 0;

	enum	json_states		state	= ctx->state;
	enum	json_modes		mode	= ctx->mode_stack[ctx->stack_top];

	enum	json_classes	chr_class	= __;
	enum	json_states		next_state	= __;

	chr_class = ((unsigned char)chr) >= 128 ? C_ETC : json_ascii_class[(int)chr];
	next_state = json_state_table[state][chr_class];

	/* pre-process: special case for integers */
	if ( (IN == state || FR == state) && (IN != next_state && FR != next_state) )
	{
		if ( NULL != ctx->target )
		{
			if ( json_type_number == json_get_type(ctx->target->value) )
			{
				if ( 0 != ctx->target->negative )
				{
					ctx->target->value->value.number.value *= -1;
				}
			}
		}
	}

	switch ( next_state )
	{
	case _O:	/* start of object */
		if ( 0 != json_push(ctx, MODE_KEY) )
		{
			switch(mode)
			{
			case MODE_ARRAY:
				assert(json_type_array == json_get_type(ctx->target->object));
				assert(NULL == ctx->target->key);
				assert(NULL == ctx->target->value);
				if ( 0 != json_push_value(ctx, json_type_object) )
				{
					result = -1;
				}
				break;

			case MODE_OBJECT:
				assert(NULL != ctx->target);
				assert(NULL != ctx->target->key);
				assert(NULL == ctx->target->value);
				if ( 0 != json_push_value(ctx, json_type_object) )
				{
					result = -1;
				}
				break;

			case MODE_DONE:
				/* it's the root object */
				if ( 0 != json_push_value(ctx, json_type_object) )
				{
					result = -1;
				}
				break;

			default:
				assert(0);
				result = -1;
				break;
			}
		}
		else
		{
			result = -1;
		}
		ctx->state = OB;
		break;

	case _A:	/* start of array */
		if ( 0 != json_push(ctx, MODE_ARRAY))
		{
			switch(mode)
			{
			case MODE_DONE:
				/* it's the root object */
				assert(NULL == ctx->target);
				ctx->target = malloc(sizeof(*ctx->target));
				if ( NULL != ctx->target )
				{
					memset(ctx->target, 0, sizeof(*ctx->target));
					ctx->target->object = json_create(json_type_array);
					if ( NULL == ctx->target->object )
					{
						assert(0);
						result = -1;
					}
				}
				else
				{
					assert(0);
					result = -1;
				}
				break;

			case MODE_OBJECT:
				assert(NULL != ctx->target);
				assert(json_type_string == json_get_type(ctx->target->key));
				assert(NULL == ctx->target->value);
				if ( 0 != json_push_value(ctx, json_type_array) )
				{
					result = -1;
				}
				break;

			case MODE_ARRAY:
				assert(NULL != ctx->target);
				assert(json_type_array == json_get_type(ctx->target->object));
				if ( 0 != json_push_value(ctx, json_type_array) )
				{
					result = -1;
				}
				break;

			default:
				/* should never reach here */
				assert(0);
				result = -1;
				break;
			}
		}
		else
		{
			result = -1;
		}
		ctx->state = AR;
		break;

	case _S:	/* end of string */
		switch (mode)
		{
		case MODE_KEY:
			ctx->state = CO;
			break;
		case MODE_ARRAY:
		case MODE_OBJECT:
			ctx->state = OK;
			break;
		default:
			assert(0);
			result = -1;
			break;
		}
		break;

	case _C:	/* colon = end of key */
		if ( 0 == json_pop(ctx, MODE_KEY) || 0 == json_push(ctx, MODE_OBJECT) )
		{
			result = -1;
		}
		ctx->state = VA;
		break;

	case _V:	/* comma = end of key-value pair, array element */
		switch (mode)
		{
		case MODE_OBJECT:
			if ( 0 != json_pop(ctx, MODE_OBJECT) && 0 != json_push(ctx, MODE_KEY))
			{
				assert(NULL != ctx->target);
				assert(json_type_object == json_get_type(ctx->target->object));
				assert(json_type_string == json_get_type(ctx->target->key));
				assert(NULL != ctx->target->value);
				json_object_set(ctx->target->object,
								ctx->target->key->value.string.value,
								ctx->target->value);
				json_destroy(ctx->target->key);
				json_destroy(ctx->target->value);
				ctx->target->key = NULL;
				ctx->target->value = NULL;
			}
			else
			{
				result = -1;
			}
			ctx->state = KE;
			break;

		case MODE_ARRAY:
			assert(json_type_array == json_get_type(ctx->target->object));
			assert(NULL == ctx->target->key);
			assert(NULL != ctx->target->value);

			json_array_append(ctx->target->object, ctx->target->value);
			json_destroy(ctx->target->value);
			ctx->target->value = NULL;
			ctx->state = VA;
			break;

		default:
			assert(0);
			result = -1;
		}
		break;

	case _T:	/* end of object */
		switch (mode)
		{
		case MODE_KEY:
			if ( 0 != json_pop(ctx, MODE_KEY) )
			{
				/* empty object */
				assert(NULL != ctx->target);
				assert(json_type_object == json_get_type(ctx->target->object));
				assert(NULL == ctx->target->key );
				assert(NULL == ctx->target->value);

				if ( NULL != ctx->target->parent )
				{
					struct json_parse_value *prev = ctx->target;
					ctx->target = prev->parent;
					ctx->target->value = prev->object;
					free(prev);
					prev = NULL;
				}
			}
			break;
		case MODE_OBJECT:
			if ( 0 != json_pop(ctx, MODE_OBJECT) )
			{
				assert(NULL != ctx->target);
				assert(json_type_object == json_get_type(ctx->target->object));
				assert(json_type_string == json_get_type(ctx->target->key));
				assert(NULL != ctx->target->value);

				json_object_set(ctx->target->object,
								ctx->target->key->value.string.value,
								ctx->target->value);
				json_destroy(ctx->target->key);
				json_destroy(ctx->target->value);
				ctx->target->key = NULL;
				ctx->target->value = NULL;
				if ( NULL != ctx->target->parent )
				{
					struct json_parse_value *prev = ctx->target;
					ctx->target = prev->parent;
					ctx->target->value = prev->object;
					free(prev);
					prev = NULL;
				}
			}
			break;
		default:
			assert(0);
			result = -1;
			break;
		}
		ctx->state = OK;
		break;

	case _Y:	/* end of array */
		if ( 0 != json_pop(ctx, MODE_ARRAY) )
		{
			assert(json_type_array == json_get_type(ctx->target->object));
			assert(NULL == ctx->target->key);
			if ( NULL != ctx->target->value )
			{
				/* non-empty array */
				json_array_append(ctx->target->object, ctx->target->value);
				json_destroy(ctx->target->value);
				ctx->target->value = NULL;
			}
			if ( NULL != ctx->target->parent )
			{
				struct json_parse_value *prev = ctx->target;
				ctx->target = prev->parent;
				ctx->target->value = prev->object;
				prev->object = NULL;
				free(prev);
			}
		}
		else
		{
			result = -1;
		}
		ctx->state = OK;
		break;

	/*************************************************************************/

	case GO:	/* start    */
	case OB:	/* object   */
	case KE:	/* key      */
	case CO:	/* colon    */
	case VA:	/* value    */
	case AR:	/* array    */
		ctx->state = next_state;
		break;

	case OK:	/* ok       */
		switch ( ctx->state )
		{
		case T3:
			assert( NULL != ctx->target );
			ctx->target->value = json_create(json_type_true);
			if ( NULL == ctx->target->value )
			{
				assert(0);
				result = -1;
			}
			break;

		case F4:
			assert( NULL != ctx->target );
			ctx->target->value = json_create(json_type_false);
			if ( NULL == ctx->target->value )
			{
				assert(0);
				result = -1;
			}
			break;

		case N3:
			assert( NULL != ctx->target );
			ctx->target->value = json_create(json_type_null);
			if ( NULL == ctx->target->value )
			{
				assert(0);
				result = -1;
			}
			break;

		default:
			break;
		}
		ctx->state = next_state;
		break;

	case ST:	/* string   */
		if ( ES == ctx->state )
		{
			/* escape character support */
			switch ( chr )
			{
			case '"':
				chr = '"';
				break;
			case '\\':
				chr = '\\';
				break;
			case '/':
				chr = '/';
				break;
			case 'b':
				chr = '\b';
				break;
			case 'f':
				chr = '\f';
				break;
			case 'n':
				chr = '\n';
				break;
			case 'r':
				chr = '\r';
				break;
			case 't':
				chr = '\t';
				break;
			default:
				assert(0);
				result = -1;
				break;
			}
		}
		if ( -1 != result )
		{
			struct json_value ** target = NULL;

			assert(NULL != ctx->target);
			switch ( mode )
			{
			case MODE_KEY:
				assert(json_type_object == json_get_type(ctx->target->object));
				assert(NULL == ctx->target->value);
				target = &(ctx->target->key);
				break;

			case MODE_OBJECT:
				assert(json_type_object == json_get_type(ctx->target->object));
				assert(json_type_string == json_get_type(ctx->target->key));
				target = &(ctx->target->value);
				break;

			case MODE_ARRAY:
				assert(json_type_array == json_get_type(ctx->target->object));
				target = &(ctx->target->value);
				break;

			default:
				/* should never reach here */
				assert(0);
				result = -1;
				break;
			}
			switch ( ctx->state )
			{
			case ST:
			case ES:	/* escape character */
				json_string_append(*target, chr);
				break;

			case U4:
				ctx->target->chr *= 16;
				if ( (chr >= '0') && (chr <= '9') )
				{
					ctx->target->chr += (chr - '0');
				}
				else if ( (chr >= 'a') && (chr <= 'f') )
				{
					ctx->target->chr += (chr - 'a' + 10);
				}
				else /* if ( (chr >= 'A') && (chr <= 'F') ) */
				{
					ctx->target->chr += (chr - 'A' + 10);
				}
				if ( ctx->target->chr > 0x07ff )
				{
					/* 3-byte character */
					json_string_append(*target, (char)(0xe0 | (ctx->target->chr >> 12)));
					json_string_append(*target, (char)(0x80 | ((ctx->target->chr >> 6) & 0x3f)));
					json_string_append(*target, (char)(0x80 | (ctx->target->chr & 0x3f)));
				}
				else if ( ctx->target->chr > 0x007f )
				{
					/* 2-byte character */
					json_string_append(*target, (char)(0xc0 | (ctx->target->chr >> 6)));
					json_string_append(*target, (char)(0x80 | (ctx->target->chr & 0x3f)));
				}
				else
				{
					/* 1-byte character */
					json_string_append(*target, (char)ctx->target->chr);
				}
				break;

			default:
				*target = json_create(json_type_string);
				if ( NULL == (*target) )
				{
					assert(0);
					result = -1;
				}
				break;
			}
			ctx->state = next_state;
		}
		break;

	case ES:	/* escape   */
	case MI:	/* minus    */
		ctx->state = next_state;
		break;

	case U1:	/* u1       */
		ctx->target->chr = 0;
		ctx->state = next_state;
		break;

	case U2:	/* u2       */
	case U3:	/* u3       */
	case U4:	/* u4       */
		ctx->target->chr *= 16;
		if ( (chr >= '0') && (chr <= '9') )
		{
			ctx->target->chr += (chr - '0');
		}
		else if ( (chr >= 'a') && (chr <= 'f') )
		{
			ctx->target->chr += (chr - 'a' + 10);
		}
		else /* if ( (chr >= 'A') && (chr <= 'F') ) */
		{
			ctx->target->chr += (chr - 'A' + 10);
		}
		ctx->state = next_state;
		break;

	case ZE:	/* zero     */
	case IN:	/* integer  */
		assert(MODE_KEY != mode);
		switch( mode )
		{
		case MODE_ARRAY:
		case MODE_OBJECT:
			assert(NULL != ctx->target);
			if ( IN != ctx->state )
			{
				assert( (MODE_OBJECT == mode) || (NULL == ctx->target->key));
				ctx->target->pow		= 0;
				ctx->target->negative	= MI == ctx->state ? 1 : 0;
				ctx->target->value		= json_create(json_type_number);
				if ( NULL == ctx->target->value )
				{
					assert(0);
					result = -1;
				}
				else if ( IN == next_state )
				{
					ctx->target->value->value.number.value = chr - '0';
				}
			}
			else
			{
				assert(NULL != ctx->target->value);
				assert( (chr >= '0') && (chr <= '9') );
				ctx->target->value->value.number.value *= 10;
				ctx->target->value->value.number.value += chr - '0';
			}
			break;

		default:
			assert(0);
			result = -1;
			break;
		}
		ctx->state = next_state;
		break;

	case FR:	/* fraction */
		if ( FR == ctx->state )
		{
			double exp = (chr - '0') * pow(10.0, ctx->target->pow);
			ctx->target->value->value.number.value += exp;
		}
		--(ctx->target->pow);
		ctx->state = next_state;
		break;

	case E2:	/* ex       */
	case E3:	/* exp      */
		/* TODO: add exp number support */
		ctx->state = next_state;
		break;

	case E1:	/* e        */
	case T1:	/* tr       */
	case T2:	/* tru      */
	case T3:	/* true     */
	case F1:	/* fa       */
	case F2:	/* fal      */
	case F3:	/* fals     */
	case F4:	/* false    */
	case N1:	/* nu       */
	case N2:	/* nul      */
	case N3:	/* null     */
		ctx->state = next_state;
		break;

	case __:	/* invalid state */
	default:
		assert(0);
		result = -1;
		break;
	}

	return result;
}


/**
 *	Create a json_context object.
 *
 *	@param[in]	depth	: maximum json depth.
 *
 *	@return		The json data model if [string] is well formatted. Otherwise it
 *				will return NULL. Use [json_destroy] to free the returned object
 *				when it's no longer needed.
 */
struct json_context * json_create_context(int depth)
{
	struct json_context * ctx = malloc(sizeof(struct json_context));

	memset(ctx, 0, sizeof(*ctx));

	ctx->mode_stack  = malloc(sizeof(ctx->mode_stack[0]) * depth);
	if ( NULL == ctx->mode_stack )
	{
		return NULL;
	}

	ctx->stack_size = depth;
	memset(ctx->mode_stack, 0, sizeof(ctx->mode_stack[0]) * depth);

	return ctx;
}


/**
 *	Stop parsing json object and determine if the parsed string is a valid and
 *	complete json expression.
 *
 *	@param[in]	ctx		: the [json_context] object
 *
 *	@return		Return 0 if a complete json expression is parsed, otherwise -1
 *				will be returned.
 */
int json_finalize_context(struct json_context * ctx)
{
    if ( (NULL != ctx) && (ctx->state == OK) && json_pop(ctx, MODE_DONE) )
	{
		return 0;
	}

    return -1;
}

/**
 *	Destroy the [json_context] object and free resources allocated for the
 *	[json_context] object.
 *
 *	@param[in]	context		: the [json_context] object to be destroyed.
 */
void json_destroy_context(struct json_context * context)
{
	struct json_parse_value * value = NULL;

	if ( NULL == context )
	{
		return;
	}

	if ( NULL != context->mode_stack )
	{
		free(context->mode_stack);
		context->mode_stack = NULL;
	}

	value = context->target;
	while ( value != NULL )
	{
		struct json_parse_value * parent = NULL;

		json_destroy(value->object);
		json_destroy(value->key);
		json_destroy(value->value);
		value->object = NULL;
		value->key = NULL;
		value->value = NULL;

		parent = value->parent;
		free(value);
		value = parent;
	}

	free(context);
	context = NULL;
}


/**
 *	Get the [json_value] object from [json_context].
 *
 *	@param[in]		ctx	:	pointer to the [json_context] object.
 *
 *	@return		Return pointer to the [json_value] object which is created from
 *				a json formatted string. Use [json_destroy] to free the returned
 *				object when is no longer needed. If any error encountered, NULL
 *				will be returned.
 */
struct json_value * json_get_value(struct json_context * ctx)
{
	if ( NULL == ctx || OK != ctx->state || NULL == ctx->target )
	{
		return NULL;
	}

	return json_duplicate(ctx->target->object);
}


/**
 *	Get size of a json array.
 *
 *	@param[in]	var		: the [json_value] object of type [json_type_array].
 *
 *	@return		Return size of the array if successful, otherwise return 0.
 */
unsigned long json_array_size(struct json_value * var)
{
	if ( json_type_array != json_get_type(var) )
	{
		return 0;
	}
	else
	{
		return var->value.array.size;
	}
}


/**
 *	Get array element from an array.
 *
 *	@param[in]		var		: the array
 *	@param[in]		idx		: 0-based array element index
 *
 *	@return		A copy of the array element will be returned on success. Use
 *				[json_destroy] to free it when it's no longer needed. Otherwise
 *				NULL will be returned.
 */
struct json_value * json_array_get(struct json_value * var, unsigned long idx)
{
	struct json_value * value = NULL;
	if ( json_type_array == json_get_type(var) && idx < json_array_size(var) )
	{
		value = json_duplicate(var->value.array.value[idx]);
	}

	return value;
}


/**
 *	Convert a json value into type 'json_type_number'.
 *
 *	@param[in/out]	var		: the json value to be converted.
 *
 *	@note		When converting string into number, only integer is supported.
 *
 *	@return		Return non-zero if it's succesfully converted, otherwise 0 will
 *				be returned.
 */
int json_to_number(struct json_value * var)
{
	int result = 0;
	enum json_value_type type = json_get_type(var);
	switch ( type )
	{
	case json_type_number:
		result = 1;
		break;

	case json_type_string:
		if ( 0 != var->value.string.size )
		{
			char * sz	= var->value.string.value;
			char * end	= NULL;
			int number	= strtol(sz, &end, 0);

			if ( (NULL != end) && ('\0' == *end ) )
			{
				free(var->value.string.value);

				var->value.string.value	= NULL;
				var->type				= json_type_number;
				var->value.number.value	= number;
				result					= 1;
			}
		}
		break;

	default:
		break;
	}

	return result;
}
