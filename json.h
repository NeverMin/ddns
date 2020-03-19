/*
 *	Copyright (C) 2009-2010 K.R.F. Studio.
 *
 *	$Id: json.h 263 2011-10-18 01:37:46Z kuang $
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
 *  Provide json format serialization support.
 */

#ifndef _INC_JSON
#define _INC_JSON

#ifdef __cplusplus
extern "C" {
#endif

/* they're defined in "json.c" to hide the details. */
struct json_value;
struct json_context;

/* json value types  */
enum json_value_type
{
	json_type_invalid	= -1,
	json_type_null		= 0,
	json_type_string,
	json_type_number,
	json_type_object,
	json_type_array,
	json_type_true,
	json_type_false
};


/**
 *	Create a json_context object.
 *
 *	@param[in]	depth	: maximum json depth.
 *
 *	@return		Return pointer to the newly created json_context object if
 *				successful. Otherwise it will return NULL.
 *				Use [json_destroy_context] to free the returned object when it's
 *				no longer needed.
 */
struct json_context * json_create_context(int depth);


/**
 *	Destroy the [json_context] object and free resources allocated for the
 *	[json_context] object.
 *
 *	@param[in]	context		: the [json_context] object to be destroyed.
 */
void json_destroy_context(struct json_context * context);


/**
 *	Stop parsing json object and determine if the parsed string is a valid and
 *	complete json expression.
 *
 *	@param[in]	ctx		: the [json_context] object
 *
 *	@return		Return 0 if a complete json expression is parsed, otherwise -1
 *				will be returned.
 */
int json_finalize_context(struct json_context * ctx);


/**
 *	Read a char into the json parse context.
 *
 *	@param[out]		ctx	: the json parse context.
 *	@param[in]		chr	: the char to be parsed.
 *
 *	@return		Return 0 if successful, otherwise return -1.
 */
int json_readchr(struct json_context * ctx, char chr);


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
struct json_value * json_get_value(struct json_context * ctx);


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
struct json_value * json_create(enum json_value_type type);


/**
 *	Destroy the [json_vlaue] object and free resources allocated for the
 *	[json_value] object.
 *
 *	@param[in]	var		: the [json_value] object to be destroyed.
 */
void json_destroy(struct json_value * var);


/**
 *	Get type of a [json_value] object.
 *
 *	@param[in]	var		: pointer to the [json_value] object.
 *
 *	@return		Return type of the [json_value] object. If invalid object is
 *				passed in, json_type_invalid will be returned.
 */
enum json_value_type json_get_type(struct json_value * var);


/**
 *	Create a copy of a [json_value] object.
 *
 *	@param[in]	var		: the object to be copied.
 *
 *	@return		Return pointer to the newly created [json_value] object. If the
 *				copy failed, NULL will be returned. When it's no longer needed,
 *				use [json_destroy] to free it.
 */
struct json_value * json_duplicate(struct json_value * var);


/**
 *	Get value of a json string object.
 *
 *	@param[in]	var		: the [json_value] object of type [json_type_string].
 *
 *	@return		Return value of the [json_value] object. If [var] is not a
 *				string, NULL will be returned.
 */
const char * json_string_get(struct json_value * var);


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
int json_string_set(struct json_value * var, char * string);


/**
 *	Get value of a json number object.
 *
 *	@param[in]	var		: the [json_value] object of type [json_type_number].
 *
 *	@return		Return value of the [json_value] object. If [var] is not a
 *				number, 0 will be returned.
 */
double json_number_get(struct json_value * var);


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
int json_number_set(struct json_value * var, int number);


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
struct json_value * json_object_get(struct json_value * var, char * key);


/**
 *	Set member value of a json object object.
 *
 *	@param[in]	var		: the [json_value] object of type [json_type_object].
 *	@param[in]	key		: name of the object member.
 *	@param[in]	value	: value of the attribute.
 *
 *	@return		Return 0 if the operation succeeded. Otherwise -1 is returned.
 */
int json_object_set(
	struct json_value	*	var,
	char				*	key,
	struct json_value	*	value
	);


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
	);


/**
 *	Get size of a json array.
 *
 *	@param[in]	var		: the [json_value] object of type [json_type_array].
 *
 *	@return		Return size of the array if successful, otherwise return 0.
 */
unsigned long json_array_size(
	struct json_value	*	var
	);


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
struct json_value * json_array_get(
	struct json_value	*	var,
	unsigned long			idx
	);
	

/**
 *	Convert a json value into type 'json_type_number'.
 *
 *	@param[in/out]	var		: the json value to be converted.
 *
 *	@note		When converting string into number, only integer is supported.
 *
 *	@return		Return non-zero if it's successfully converted, otherwise 0 will
 *				be returned.
 */
int json_to_number(struct json_value * var);


#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif	/* _INC_JSON */
