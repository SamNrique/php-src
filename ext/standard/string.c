/*
   +----------------------------------------------------------------------+
   | PHP HTML Embedded Scripting Language Version 3.0                     |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997,1998 PHP Development Team (See Credits file)      |
   +----------------------------------------------------------------------+
   | This program is free software; you can redistribute it and/or modify |
   | it under the terms of one of the following licenses:                 |
   |                                                                      |
   |  A) the GNU General Public License as published by the Free Software |
   |     Foundation; either version 2 of the License, or (at your option) |
   |     any later version.                                               |
   |                                                                      |
   |  B) the PHP License as published by the PHP Development Team and     |
   |     included in the distribution in the file: LICENSE                |
   |                                                                      |
   | This program is distributed in the hope that it will be useful,      |
   | but WITHOUT ANY WARRANTY; without even the implied warranty of       |
   | MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        |
   | GNU General Public License for more details.                         |
   |                                                                      |
   | You should have received a copy of both licenses referred to here.   |
   | If you did not, or have any questions about PHP licensing, please    |
   | contact core@php.net.                                                |
   +----------------------------------------------------------------------+
   | Authors: Rasmus Lerdorf <rasmus@lerdorf.on.ca>                       |
   |          Stig S�ther Bakken <ssb@guardian.no>                        |
   |          Zeev Suraski <bourbon@nevision.net.il>                      |
   +----------------------------------------------------------------------+
 */


/* $Id$ */

#include <stdio.h>
#include "php.h"
#include "reg.h"
#include "post.h"
#include "php3_string.h"
#if HAVE_SETLOCALE
#include <locale.h>
#endif
#include "zend_execute.h"
#include "php_globals.h"


/* {{{ proto int strlen(string str)
   Get string length */
void php3_strlen(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *str;
	TLS_VARS;
	
	if (ARG_COUNT(ht) != 1 || getParameters(ht, 1, &str) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(str);
	RETVAL_LONG(str->value.str.len);
}
/* }}} */

/* {{{ proto int strcmp(string str1, string str2)
   Binary safe string comparison */
void php3_strcmp(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *s1,*s2;
	
	if (ARG_COUNT(ht) != 2 || getParameters(ht, 2, &s1, &s2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(s1);
	convert_to_string(s2);
	RETURN_LONG(php3_binary_strcmp(s1,s2));
}
/* }}} */

/* {{{ proto int strcasecmp(string str1, string str2)
   Binary safe case-insensitive string comparison */
void php3_strcasecmp(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *s1,*s2;
	
	if (ARG_COUNT(ht)!=2 || getParameters(ht, 2, &s1, &s2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(s1);
	convert_to_string(s2);
	RETURN_LONG(strcasecmp(s1->value.str.val,s2->value.str.val));
}
/* }}} */

/* {{{ proto int strspn(string str, string mask)
   Find length of initial segment consisting entirely of characters found in mask */
void php3_strspn(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *s1,*s2;
	
	if (ARG_COUNT(ht)!=2 || getParameters(ht, 2, &s1, &s2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(s1);
	convert_to_string(s2);
	RETURN_LONG(strspn(s1->value.str.val,s2->value.str.val));
}
/* }}} */

/* {{{ proto int strcspn(string str, string mask)
   Find length of initial segment consisting entirely of characters not found in mask */
void php3_strcspn(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *s1,*s2;
	
	if (ARG_COUNT(ht)!=2 || getParameters(ht, 2, &s1, &s2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(s1);
	convert_to_string(s2);
	RETURN_LONG(strcspn(s1->value.str.val,s2->value.str.val));
}
/* }}} */

/* {{{ proto string chop(string str)
   Remove trailing whitespace */
void php3_chop(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *str;
	register int i;
	TLS_VARS;
	
	if (ARG_COUNT(ht) != 1 || getParameters(ht, 1, &str) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(str);

	if (str->type == IS_STRING) {
		int len = str->value.str.len;
		char *c = str->value.str.val;
		for (i = len - 1; i >= 0; i--) {
			if (c[i] == ' ' || c[i] == '\n' || c[i] == '\r' ||
				c[i] == '\t' || c[i] == '\v') {
				len--;
			} else {
				break;
			}
		}
		RETVAL_STRINGL(c, len, 1);
		return;
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto string trim(string str)
   Strip whitespace from the beginning and end of a string */
void php3_trim(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *str;
	register int i;
	TLS_VARS;
	
	if (ARG_COUNT(ht) != 1 || getParameters(ht, 1, &str) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(str);

	if (str->type == IS_STRING) {
		int len = str->value.str.len;
		int trimmed = 0;
		char *c = str->value.str.val;
		for (i = 0; i < len; i++) {
			if (c[i] == ' ' || c[i] == '\n' || c[i] == '\r' ||
				c[i] == '\t' || c[i] == '\v') {
				trimmed++;
			} else {
				break;
			}
		}
		len-=trimmed;
		c+=trimmed;
		for (i = len - 1; i >= 0; i--) {
			if (c[i] == ' ' || c[i] == '\n' || c[i] == '\r' ||
				c[i] == '\t' || c[i] == '\v') {
				len--;
			} else {
				break;
			}
		}
		RETVAL_STRINGL(c, len, 1);
		return;
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto string ltrim(string str)
   Strip whitespace from the beginning of a string */
void php3_ltrim(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *str;
	register int i;
	TLS_VARS;
	
	if (ARG_COUNT(ht) != 1 || getParameters(ht, 1, &str) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(str);

	if (str->type == IS_STRING) {
		int len = str->value.str.len;
		int trimmed = 0;
		char *c = str->value.str.val;
		for (i = 0; i < len; i++) {
			if (c[i] == ' ' || c[i] == '\n' || c[i] == '\r' ||
				c[i] == '\t' || c[i] == '\v') {
				trimmed++;
			} else {
				break;
			}
		}
		RETVAL_STRINGL(c+trimmed, len-trimmed, 1);
		return;
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto array(string separator, string str)
   Split a string on string separator and return array of components */
void php3_explode(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *str, *delim;
	char *work_str, *p1, *p2;
	int i = 0;

	if (ARG_COUNT(ht) != 2 || getParameters(ht, 2, &delim, &str) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(str);
	convert_to_string(delim);

	if (strlen(delim->value.str.val)==0) {
		/* the delimiter must be a valid C string that's at least 1 character long */
		php3_error(E_WARNING,"Empty delimiter");
		RETURN_FALSE;
	}
	if (array_init(return_value) == FAILURE) {
		return;
	}
	work_str = p1 = estrndup(str->value.str.val,str->value.str.len);
	p2 = strstr(p1, delim->value.str.val);
	if (p2 == NULL) {
		add_index_string(return_value, i++, p1, 1);
	} else do {
		p2[0] = 0;
		add_index_string(return_value, i++, p1, 1);
		p1 = p2 + delim->value.str.len;
	} while ((p2 = strstr(p1, delim->value.str.val)) && p2 != work_str);
	if (p1 != work_str) {
		add_index_string(return_value, i++, p1, 1);
	}
	efree(work_str);
}
/* }}} */

/* {{{ proto string implode(array src, string glue)
   Join array elements placing glue string between items and return one string */
void php3_implode(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *arg1, *arg2, *delim, *tmp, *arr;
	int len = 0, count = 0;
	TLS_VARS;
	
	if (ARG_COUNT(ht) != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	if (arg1->type == IS_ARRAY && arg2->type == IS_STRING) {
		arr = arg1;
		delim = arg2;
	} else if (arg2->type == IS_ARRAY) {
		convert_to_string(arg1);
		arr = arg2;
		delim = arg1;
	} else {
		php3_error(E_WARNING, "Bad arguments to %s()",
				   get_active_function_name());
		return;
	}

	/* convert everything to strings, and calculate length */
	_php3_hash_internal_pointer_reset(arr->value.ht);
	while (_php3_hash_get_current_data(arr->value.ht, (void **) &tmp) == SUCCESS) {
		convert_to_string(tmp);
		if (tmp->type == IS_STRING) {
			len += tmp->value.str.len;
			if (count>0) {
				len += delim->value.str.len;
			}
			count++;
		}
		_php3_hash_move_forward(arr->value.ht);
	}

	/* do it */
	return_value->value.str.val = (char *) emalloc(len + 1);
	return_value->value.str.val[0] = '\0';
	return_value->value.str.val[len] = '\0';
	_php3_hash_internal_pointer_reset(arr->value.ht);
	while (_php3_hash_get_current_data(arr->value.ht, (void **) &tmp) == SUCCESS) {
		if (tmp->type == IS_STRING) {
			count--;
			strcat(return_value->value.str.val, tmp->value.str.val);
			if (count > 0) {
				strcat(return_value->value.str.val, delim->value.str.val);
			}
		}
		_php3_hash_move_forward(arr->value.ht);
	}
	return_value->type = IS_STRING;
	return_value->value.str.len = len;
}
/* }}} */

#ifndef THREAD_SAFE
char *strtok_string;
#endif

/* {{{ proto string strtok([string str,] string token)
   Tokenize a string */
void php3_strtok(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *str, *tok;
#ifndef THREAD_SAFE
	static char *strtok_pos1 = NULL;
	static char *strtok_pos2 = NULL;
#endif
	char *token = NULL, *tokp=NULL;
	char *first = NULL;
	int argc;
	TLS_VARS;
	
	argc = ARG_COUNT(ht);

	if ((argc == 1 && getParameters(ht, 1, &tok) == FAILURE) ||
		(argc == 2 && getParameters(ht, 2, &str, &tok) == FAILURE) ||
		argc < 1 || argc > 2) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(tok);
	tokp = token = tok->value.str.val;

	if (argc == 2) {
		convert_to_string(str);

		STR_FREE(GLOBAL(strtok_string));
		GLOBAL(strtok_string) = estrndup(str->value.str.val,str->value.str.len);
		STATIC(strtok_pos1) = GLOBAL(strtok_string);
		STATIC(strtok_pos2) = NULL;
	}
	if (STATIC(strtok_pos1) && *STATIC(strtok_pos1)) {
		for ( /* NOP */ ; token && *token; token++) {
			STATIC(strtok_pos2) = strchr(STATIC(strtok_pos1), (int) *token);
			if (!first || (STATIC(strtok_pos2) && STATIC(strtok_pos2) < first)) {
				first = STATIC(strtok_pos2);
			}
		}						/* NB: token is unusable now */

		STATIC(strtok_pos2) = first;
		if (STATIC(strtok_pos2)) {
			*STATIC(strtok_pos2) = '\0';
		}
		RETVAL_STRING(STATIC(strtok_pos1),1);
#if 0
		/* skip 'token' white space for next call to strtok */
		while (STATIC(strtok_pos2) && 
			strchr(tokp, *(STATIC(strtok_pos2)+1))) {
			STATIC(strtok_pos2)++;
		}
#endif
		if (STATIC(strtok_pos2))
			STATIC(strtok_pos1) = STATIC(strtok_pos2) + 1;
		else
			STATIC(strtok_pos1) = NULL;
	} else {
		RETVAL_FALSE;
	}
}
/* }}} */

PHPAPI char *_php3_strtoupper(char *s)
{
	char *c;
	int ch;

	c = s;
	while (*c) {
		ch = toupper((unsigned char)*c);
		*c++ = ch;
	}
	return (s);
}

/* {{{ proto string strtoupper(string str)
   Make a string uppercase */
void php3_strtoupper(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *arg;
	char *ret;
	TLS_VARS;
	
	if (ARG_COUNT(ht) != 1 || getParameters(ht, 1, &arg)) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(arg);

	ret = _php3_strtoupper(arg->value.str.val);
	RETVAL_STRING(ret,1);
}
/* }}} */


PHPAPI char *_php3_strtolower(char *s)
{
	register int ch;
	char *c;

	c = s;
	while (*c) {
		ch = tolower((unsigned char)*c);
		*c++ = ch;
	}
	return (s);
}

/* {{{ proto string strtolower(string str)
   Make a string lowercase */
void php3_strtolower(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *str;
	char *ret;
	TLS_VARS;
	
	if (ARG_COUNT(ht) != 1 || getParameters(ht, 1, &str)) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(str);

	ret = _php3_strtolower(str->value.str.val);
	RETVAL_STRING(ret,1);
}
/* }}} */

/* {{{ proto string basename(string path)
   Return the filename component of the path */
void php3_basename(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *str;
	char *ret, *c;
	TLS_VARS;
	
	if (ARG_COUNT(ht) != 1 || getParameters(ht, 1, &str)) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(str);
	ret = estrdup(str->value.str.val);
	c = ret + str->value.str.len -1;	
	while (*c == '/'
#ifdef MSVC5
		   || *c == '\\'
#endif
		)
		c--;
	*(c + 1) = '\0';	
	if ((c = strrchr(ret, '/'))
#ifdef MSVC5
		|| (c = strrchr(ret, '\\'))
#endif
		) {
		RETVAL_STRING(c + 1,1);
	} else {
		RETVAL_STRING(str->value.str.val,1);
	}
	efree(ret);
}
/* }}} */

PHPAPI void _php3_dirname(char *str, int len) {
	register char *c;

	c = str + len - 1;
	while (*c == '/'
#ifdef MSVC5
		   || *c == '\\'
#endif
		)
		c--; /* strip trailing slashes */
	*(c + 1) = '\0';
	if ((c = strrchr(str, '/'))
#ifdef MSVC5
		|| (c = strrchr(str, '\\'))
#endif
		)
		*c='\0';
}

/* {{{ proto string dirname(string path)
   Return the directory name component of the path */
void php3_dirname(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *str;
	char *ret;
	TLS_VARS;
	
	if (ARG_COUNT(ht) != 1 || getParameters(ht, 1, &str)) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(str);
	ret = estrdup(str->value.str.val);
	_php3_dirname(ret,str->value.str.len);
	RETVAL_STRING(ret,1);
	efree(ret);
}
/* }}} */


/* case-insensitve strstr */
PHPAPI char *php3i_stristr(unsigned char *s, unsigned char *t)
{
	int i, j, k, l;
	
	for (i = 0; s[i]; i++) {
		for (j = 0, l = k = i; s[k] && t[j] &&
			tolower(s[k]) == tolower(t[j]); j++, k++)
			;
		if (t[j] == '\0')
			return s + l;
	}
	return NULL;
}

/* {{{ proto string strstr(string haystack, string needle)
   Find first occurrence of a string within another, case insensitive */
void php3_stristr(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *haystack, *needle;
	char *found = NULL;
	TLS_VARS;
	
	if (ARG_COUNT(ht) != 2 || getParameters(ht, 2, &haystack, &needle) ==
		FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(haystack);
	convert_to_string(needle);

	if (strlen(needle->value.str.val)==0) {
		php3_error(E_WARNING,"Empty delimiter");
		RETURN_FALSE;
	}
	found = php3i_stristr(haystack->value.str.val, needle->value.str.val);

	if (found) {
		RETVAL_STRING(found,1);
	} else {
		RETVAL_FALSE;
	}
}
/* }}} */

/* {{{ proto string strstr(string haystack, string needle)
   Find first occurrence of a string within another */
void php3_strstr(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *haystack, *needle;
	char *found = NULL;
	TLS_VARS;
	
	if (ARG_COUNT(ht) != 2 || getParameters(ht, 2, &haystack, &needle) ==
		FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(haystack);

	if (needle->type == IS_STRING) {
		if (strlen(needle->value.str.val)==0) {
			php3_error(E_WARNING,"Empty delimiter");
			RETURN_FALSE;
		}
		found = strstr(haystack->value.str.val, needle->value.str.val);
	} else {
		convert_to_long(needle);
		found = strchr(haystack->value.str.val, (char) needle->value.lval);
	}


	if (found) {
		RETVAL_STRING(found,1);
	} else {
		RETVAL_FALSE;
	}
}
/* }}} */

/* {{{ proto int strpos(string haystack, string needle)
   Find position of first occurrence of a string within another */
void php3_strpos(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *haystack, *needle, *OFFSET;
	int offset = 0;
	char *found = NULL;
	TLS_VARS;
	
	switch(ARG_COUNT(ht)) {
	case 2:
		if (getParameters(ht, 2, &haystack, &needle) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		break;
	case 3:
		if (getParameters(ht, 3, &haystack, &needle, &OFFSET) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		convert_to_long(OFFSET);
		offset = OFFSET->value.lval;
		break;
	default:
		WRONG_PARAM_COUNT;
	}
	convert_to_string(haystack);
	if (offset > haystack->value.str.len) {
		php3_error(E_WARNING,"offset not contained in string");
		RETURN_FALSE;
	}

	if (needle->type == IS_STRING) {
		if (needle->value.str.len==0) {
			php3_error(E_WARNING,"Empty delimiter");
			RETURN_FALSE;
		}
		found = strstr(haystack->value.str.val+offset, needle->value.str.val);
	} else {
		convert_to_long(needle);
		found = strchr(haystack->value.str.val+offset, (char) needle->value.lval);
	}

	if (found) {
		RETVAL_LONG(found - haystack->value.str.val);
	} else {
		RETVAL_FALSE;
	}
}
/* }}} */

/* {{{ proto int strrpos(string haystack, string needle)
   Find the last occurrence of a character in a string within another */
void php3_strrpos(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *haystack, *needle;
	char *found = NULL;
	TLS_VARS;
	
	if (ARG_COUNT(ht) != 2 || getParameters(ht, 2, &haystack, &needle) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(haystack);

	if (needle->type == IS_STRING) {
		found = strrchr(haystack->value.str.val, *needle->value.str.val);
	} else {
		convert_to_long(needle);
		found = strrchr(haystack->value.str.val, (char) needle->value.lval);
	}

	if (found) {
		RETVAL_LONG(haystack->value.str.len - strlen(found));
	} else {
		RETVAL_FALSE;
	}
}
/* }}} */

/* {{{ proto string strrchr(string haystack, string needle)
   Find the last occurrence of a character in a string within another */
void php3_strrchr(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *haystack, *needle;
	char *found = NULL;
	TLS_VARS;
	
	if (ARG_COUNT(ht) != 2 || getParameters(ht, 2, &haystack, &needle) ==
		FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(haystack);

	if (needle->type == IS_STRING) {
		found = strrchr(haystack->value.str.val, *needle->value.str.val);
	} else {

		convert_to_long(needle);
		found = strrchr(haystack->value.str.val, needle->value.lval);
	}


	if (found) {
		RETVAL_STRING(found,1);
	} else {
		RETVAL_FALSE;
	}
}
/* }}} */

static char *
_php3_chunk_split(char *src, int srclen, char *end, int endlen, int chunklen)
{
	char *dest;
	char *p, *q;
	int chunks; /* complete chunks! */
	int restlen;

	chunks = srclen / chunklen;
	restlen = srclen - chunks * chunklen; /* srclen % chunklen */

	dest = emalloc((srclen + (chunks + 1) * endlen + 1) * sizeof(char));

	for(p = src, q = dest; p < (src + srclen - chunklen + 1); ) {
		memcpy(q, p, chunklen);
		q += chunklen;
		memcpy(q, end, endlen);
		q += endlen;
		p += chunklen;
	}

	if(restlen) {
		memcpy(q, p, restlen);
		q += restlen;
		memcpy(q, end, endlen);
		q += endlen;
	}

	*q = '\0';

	return(dest);
}

/* {{{ proto string chunk_split(string str [, int chunklen [, string ending]])
   Return split line */
void php3_chunk_split(INTERNAL_FUNCTION_PARAMETERS) 
{
	pval *p_str, *p_chunklen, *p_ending;
	int argc;
	char *result;
	char *end    = "\r\n";
	int endlen   = 2;
	int chunklen = 76;
	TLS_VARS;

	argc = ARG_COUNT(ht);

	if(!((argc == 1 && getParameters(ht, 1, &p_str) != FAILURE) ||
		(argc == 2 && getParameters(ht, 2, &p_str, &p_chunklen) != FAILURE) ||
		(argc == 3 && getParameters(ht, 3, &p_str, &p_chunklen,
								   &p_ending) != FAILURE))) {
		WRONG_PARAM_COUNT;
	}
	
	switch(argc) {
		case 3:
			convert_to_string(p_ending);
			end    = p_ending->value.str.val;
			endlen = p_ending->value.str.len;
		case 2:
			convert_to_long(p_chunklen);
			chunklen = p_chunklen->value.lval;
		case 1:
			convert_to_string(p_str);
	}
			
	if(chunklen == 0) {
		php3_error(E_WARNING, "chunk length is 0");
		RETURN_FALSE;
	}
	
	result = _php3_chunk_split(p_str->value.str.val, p_str->value.str.len,
			end, endlen, chunklen);
	
	if(result) {
		RETVAL_STRING(result, 0);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto string substr(string str, int start [, int length])
   Return part of a string */
void php3_substr(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *string, *from, *len;
	int argc, l;
	int f;
	TLS_VARS;
	
	argc = ARG_COUNT(ht);

	if ((argc == 2 && getParameters(ht, 2, &string, &from) == FAILURE) ||
		(argc == 3 && getParameters(ht, 3, &string, &from, &len) == FAILURE) ||
		argc < 2 || argc > 3) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(string);
	convert_to_long(from);
	f = from->value.lval;

	if (argc == 2) {
		l = string->value.str.len;
	} else {
		convert_to_long(len);
		l = len->value.lval;
	}

	/* if "from" position is negative, count start position from the end
	 * of the string
	 */
	if (f < 0) {
		f = string->value.str.len + f;
		if (f < 0) {
			f = 0;
		}
	}

	/* if "length" position is negative, set it to the length
	 * needed to stop that many chars from the end of the string
	 */
	if (l < 0) {
		l = (string->value.str.len - f) + l;
		if (l < 0) {
			l = 0;
		}
	}

	if (f >= (int)string->value.str.len) {
		RETURN_FALSE;
	}

	if ((f + l) < (int)string->value.str.len) {
		string->value.str.val[f + l] = '\0';
	}
	RETVAL_STRING(string->value.str.val + f,1);
}
/* }}} */

/* {{{ proto string quotemeta(string str)
   Quote meta characters */
void php3_quotemeta(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *arg;
	char *str, *old;
	char *p, *q;
	char c;
	TLS_VARS;
	
	if (ARG_COUNT(ht) != 1 || getParameters(ht, 1, &arg) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(arg);

	old = arg->value.str.val;

	if (!*old) {
		RETURN_FALSE;
	}
	
	str = emalloc(2 * arg->value.str.len + 1);
	
	for(p = old, q = str; (c = *p); p++) {
		switch(c) {
			case '.':
			case '\\':
			case '+':
			case '*':
			case '?':
			case '[':
			case '^':
			case ']':
			case '$':
			case '(':
			case ')':
				*q++ = '\\';
				/* break is missing _intentionally_ */
			default:
				*q++ = c;
		}
	}
	*q = 0;
	RETVAL_STRING(erealloc(str, q - str + 1), 0);
}
/* }}} */

/* {{{ proto int ord(string character)
   Return ASCII value of character */
void php3_ord(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *str;
	TLS_VARS;
	
	if (ARG_COUNT(ht) != 1 || getParameters(ht, 1, &str) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(str);
	RETVAL_LONG((unsigned char)str->value.str.val[0]);
}
/* }}} */

/* {{{ proto string chr(int ascii)
   Convert ASCII code to a character */
void php3_chr(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *num;
	char temp[2];
	TLS_VARS;
	
	if (ARG_COUNT(ht) != 1 || getParameters(ht, 1, &num) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_long(num);
	temp[0] = (char) num->value.lval;
	temp[1] = '\0';
	RETVAL_STRINGL(temp, 1,1);
}
/* }}} */

/* {{{ proto string(string str)
   Make a string's first character uppercase */
void php3_ucfirst(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *arg;
	
	if (ARG_COUNT(ht) != 1 || getParameters(ht, 1, &arg) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(arg);

	if (!*arg->value.str.val) {
		RETURN_FALSE;
	}
	*arg->value.str.val = toupper((unsigned char)*arg->value.str.val);
	RETVAL_STRING(arg->value.str.val,1);
}
/* }}} */

/* {{{ proto string ucwords(string str)
   Uppercase the first character of every word in a string */
void php3_ucwords(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *arg;
	char *r;
	
	if (ARG_COUNT(ht) != 1 || getParameters(ht, 1, &arg) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(arg);

	if (!*arg->value.str.val) {
		RETURN_FALSE;
	}
	*arg->value.str.val = toupper((unsigned char)*arg->value.str.val);
	r=arg->value.str.val;
	while((r=strstr(r," "))){
		if(*(r+1)){
			r++;
			*r=toupper((unsigned char)*r);
		} else break;
	}
	RETVAL_STRING(arg->value.str.val,1);
}
/* }}} */

/* {{{ proto string strtr(string str, string from, string to)
   Translate characters in str using given translation tables */
void php3_strtr(INTERNAL_FUNCTION_PARAMETERS)
{								/* strtr(STRING,FROM,TO) */
	pval *str, *from, *to;
	unsigned char xlat[256];
	unsigned char *str_from, *str_to, *string;
	int i, len1, len2;
	TLS_VARS;
	
	if (ARG_COUNT(ht) != 3 || getParameters(ht, 3, &str, &from, &to) ==
		FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(str);
	convert_to_string(from);
	convert_to_string(to);

	string = (unsigned char*) str->value.str.val;
	str_from = (unsigned char*) from->value.str.val;
	str_to = (unsigned char*) to->value.str.val;

	len1 = from->value.str.len;
	len2 = to->value.str.len;

	if (len1 > len2) {
		str_from[len2] = '\0';
		len1 = len2;
	}
	for (i = 0; i < 256; xlat[i] = i, i++);

	for (i = 0; i < len1; i++) {
		xlat[(unsigned char) str_from[i]] = str_to[i];
	}

	for (i = 0; i < str->value.str.len; i++) {
		string[i] = xlat[(unsigned char) string[i]];
	}

	RETVAL_STRING((char *)string,1);
}
/* }}} */


/* {{{ proto string strrev(string str)
   Reverse a string */
void php3_strrev(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *str;
	int i,len;
	char c;
	
	if (ARG_COUNT(ht)!=1 || getParameters(ht, 1, &str)==FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	convert_to_string(str);
	
	len = str->value.str.len;
	
	for (i=0; i<len-1-i; i++) {
		c=str->value.str.val[i];
		str->value.str.val[i] = str->value.str.val[len-1-i];
		str->value.str.val[len-1-i]=c;
	}

	*return_value = *str;
	pval_copy_constructor(return_value);
}
/* }}} */


/* be careful, this edits the string in-place */
PHPAPI void _php3_stripslashes(char *string, int *len)
{
	char *s, *t;
	int l;
	char escape_char='\\';
	PLS_FETCH();

	if (PG(magic_quotes_sybase)) {
		escape_char='\'';
	}

	if (len != NULL) {
		l = *len;
	} else {
		l = strlen(string);
	}
	s = string;
	t = string;
	while (l > 0) {
		if (*t == escape_char) {
			t++;				/* skip the slash */
			if (len != NULL)
				(*len)--;
			l--;
			if (l > 0) {
				if(*t=='0') {
					*s++='\0';
					t++;
				} else {
					*s++ = *t++;	/* preserve the next character */
				}
				l--;
			}
		} else {
			if (s != t)
				*s++ = *t++;
			else {
				s++;
				t++;
			}
			l--;
		}
	}
	if (s != t) {
		*s = '\0';
	}
}

/* {{{ proto string addslashes(string str)
   Escape single quote, double quotes and backslash characters in a string with backslashes */
void php3_addslashes(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *str;

	if (ARG_COUNT(ht) != 1 || getParameters(ht, 1, &str) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(str);
	return_value->value.str.val = _php3_addslashes(str->value.str.val,str->value.str.len,&return_value->value.str.len,0);
	return_value->type = IS_STRING;
}
/* }}} */

/* {{{ proto string stripslashes(string str)
   Strip backslashes from a string */
void php3_stripslashes(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *str;
	TLS_VARS;
	
	if (ARG_COUNT(ht) != 1 || getParameters(ht, 1, &str) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(str);

	/* let RETVAL do the estrdup() */
	RETVAL_STRING(str->value.str.val,1);
	_php3_stripslashes(return_value->value.str.val,&return_value->value.str.len);
}
/* }}} */

#ifndef HAVE_STRERROR
#if !APACHE
char *strerror(int errnum) 
{
	extern int sys_nerr;
	extern char *sys_errlist[];
#ifndef THREAD_SAFE
	static char str_ebuf[40];
#endif

	if ((unsigned int)errnum < sys_nerr) return(sys_errlist[errnum]);
	(void)sprintf(STATIC(str_ebuf), "Unknown error: %d", errnum);
	return(STATIC(str_ebuf));
}
#endif
#endif


PHPAPI char *_php3_addslashes(char *str, int length, int *new_length, int should_free)
{
	/* maximum string length, worst case situation */
	char *new_str = (char *) emalloc((length?length:strlen(str))*2+1); 
	char *source,*target;
	char *end;
	char c;
	PLS_FETCH();
	
	for (source=str,end=source+length,target=new_str; (c = *source) || source<end; source++) {
		switch(c) {
			case '\0':
				*target++ = '\\';
				*target++ = '0';
				break;
			case '\'':
				if (PG(magic_quotes_sybase)) {
					*target++ = '\'';
					*target++ = '\'';
					break;
				}
				/* break is missing *intentionally* */
			case '\"':
			case '\\':
				if (!PG(magic_quotes_sybase)) {
					*target++ = '\\';
				}
				/* break is missing *intentionally* */
			default:
				*target++ = c;
				break;
		}
	}
	*target = 0;
	if(new_length) *new_length = target - new_str;
	if (should_free) {
		STR_FREE(str);
	}
	return new_str;
}


#define _HEB_BLOCK_TYPE_ENG 1
#define _HEB_BLOCK_TYPE_HEB 2
#define isheb(c) (((((unsigned char) c)>=224) && (((unsigned char) c)<=250)) ? 1 : 0)
#define _isblank(c) (((((unsigned char) c)==' ' || ((unsigned char) c)=='\t')) ? 1 : 0)
#define _isnewline(c) (((((unsigned char) c)=='\n' || ((unsigned char) c)=='\r')) ? 1 : 0)

static void _php3_char_to_str(char *str,uint len,char from,char *to,int to_len,pval *result)
{
	int char_count=0;
	char *source,*target,*tmp,*source_end=str+len, *tmp_end=NULL;
	
	for (source=str; source<source_end; source++) {
		if (*source==from) {
			char_count++;
		}
	}

	result->type = IS_STRING;
		
	if (char_count==0) {
		result->value.str.val = estrndup(str,len);
		result->value.str.len = len;
		return;
	}
	
	result->value.str.len = len+char_count*(to_len-1);
	result->value.str.val = target = (char *) emalloc(result->value.str.len+1);
	
	for (source=str; source<source_end; source++) {
		if (*source==from) {
			for (tmp=to,tmp_end=tmp+to_len; tmp<tmp_end; tmp++) {
				*target = *tmp;
				target++;
			}
		} else {
			*target = *source;
			target++;
		}
	}
	*target = 0;
}

#if 0
/*
 * this is a binary safe equivalent to strnstr
 * note that we don't check for the end in str_to_str but here
 */

static inline char *
_php3_memnstr(char *haystack, char *needle, int needle_len, char *end)
{
	char *p = haystack;
	char *s = NULL;

	for(; p < end - needle_len + 1&& 
			(s = memchr(p, *needle, end - haystack)); p = s + 1) {
		if(memcmp(s, needle, needle_len) == 0)
			return s;
	}
	return NULL;
}

/*
 * because of efficiency we use malloc/realloc/free here
 * erealloc _will_ move your data around - it took me some time
 * to find out ... Sascha Schumann <sas@schell.de> 981220
 */

static char *_php3_str_to_str(char *haystack, int length, 
	char *needle, int needle_len, char *str, int str_len, int *_new_length)
{
	char *p, *q;
	char *r, *s;
	char *end = haystack + length;
	char *new;
	
	new = malloc(length);
	/* we jump through haystack searching for the needle. hurray! */
	for(p = haystack, q = new;
			(r = _php3_memnstr(p, needle, needle_len, end));) {
	/* this ain't optimal. you could call it `efficient memory usage' */
		realloc(new, (q - new) + (r - p) + (str_len) + 1);
		memcpy(q, p, r - p);
		q += r - p;
		memcpy(q, str, str_len);
		q += str_len;
		p = r + needle_len;
	}
	/* if there is a rest, copy it */
	if((end - p)) {
		s = (q) + (end - p);
		new = realloc(new, s - new + 1);
		memcpy(q, p, end - p);
		q = s;
	}
	*q = '\0';
	if(_new_length) *_new_length = q - new;
	return new;
}
#endif

static char *_php3_memstr(char *s, char *c, size_t n, size_t m)
{
    char *p;

    for(p = s; ((size_t) (p - s)) < n; p++)
        if(memcmp(p, c, m) == 0)
            return p;
    return NULL;
}

#define ATTCHSTR(st, sz) \
    nl += sz; \
    n = realloc(n, nl + 1); \
    memcpy(n + no, st, sz); \
    no += sz


static char *_php3_str_to_str(char *a, int al, char *b, int bl, char *c, int cl,
        int *newlen)
{
    char *n = NULL, *p, *q;
    int nl = 0;
    int no = 0;

    /* run through all occurences of b in a */
    for(p = q = a; (p = _php3_memstr(p, b, al - (p - a), bl)); q = p) {
        /* attach everything between the previous occ. and this one */
        ATTCHSTR(q, p - q);
        /* attach the replacement string c */
        ATTCHSTR(c, cl);
        /* jump over string b in a */
        p += bl;
    }
    
	/* anything left over ? */
    if((al - (q - a)) > 0) {
        ATTCHSTR(q, al - (q - a));
    }

    if(newlen) *newlen = nl;
    n[nl] = '\0';

    return n;
}

#undef ATTCHSTR

/* {{{ proto string str_replace(string needle, string str, string haystack)
   Replace all occurrences of needle in haystack with str */
void php3_str_replace(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *haystack, *needle, *str;
	char *new;

	if(ARG_COUNT(ht) != 3 || 
			getParameters(ht, 3, &needle, &str, &haystack) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_string(haystack);
	convert_to_string(needle);
	convert_to_string(str);

	if(needle->value.str.len == 1) {
		_php3_char_to_str(haystack->value.str.val,haystack->value.str.len,needle->value.str.val[0],str->value.str.val, str->value.str.len ,return_value);
		return;
	}

	new = _php3_str_to_str(haystack->value.str.val, haystack->value.str.len, 
				needle->value.str.val, needle->value.str.len, 
				str->value.str.val, str->value.str.len,
				&return_value->value.str.len);
	return_value->value.str.val = emalloc(return_value->value.str.len + 1);
	memcpy(return_value->value.str.val, new, return_value->value.str.len + 1);
	free(new);
	return_value->type = IS_STRING;
}
/* }}} */

/* Converts Logical Hebrew text (Hebrew Windows style) to Visual text
 * Cheers/complaints/flames - Zeev Suraski <zeev@php.net>
 */
static void _php3_hebrev(INTERNAL_FUNCTION_PARAMETERS,int convert_newlines)
{
	pval *str,*max_chars_per_line;
	char *heb_str,*tmp,*target,*opposite_target,*broken_str;
	int block_start, block_end, block_type, block_length, i;
	int block_ended;
	long max_chars=0;
	int begin,end,char_count,orig_begin;

	
	switch(ARG_COUNT(ht)) {
		case 1:
			if (getParameters(ht, 1, &str)==FAILURE) {
				RETURN_FALSE;
			}
			break;
		case 2:
			if (getParameters(ht, 2, &str, &max_chars_per_line)==FAILURE) {
				RETURN_FALSE;
			}
			convert_to_long(max_chars_per_line);
			max_chars = max_chars_per_line->value.lval;
			break;
		default:
			WRONG_PARAM_COUNT;
			break;
	}
	
	convert_to_string(str);
	
	if (str->value.str.len==0) {
		RETURN_FALSE;
	}

	tmp = str->value.str.val;
	block_start=block_end=0;
	block_ended=0;

	heb_str = (char *) emalloc(str->value.str.len+1);
	target = heb_str+str->value.str.len;
	opposite_target = heb_str;
	*target = 0;
	target--;

	block_length=0;

	if (isheb(*tmp)) {
		block_type = _HEB_BLOCK_TYPE_HEB;
	} else {
		block_type = _HEB_BLOCK_TYPE_ENG;
	}
	
	do {
		if (block_type==_HEB_BLOCK_TYPE_HEB) {
			while((isheb((int)*(tmp+1)) || _isblank((int)*(tmp+1)) || ispunct((int)*(tmp+1)) || (int)*(tmp+1)=='\n' ) && block_end<str->value.str.len-1) {
				tmp++;
				block_end++;
				block_length++;
			}
			for (i=block_start; i<=block_end; i++) {
				*target = str->value.str.val[i];
				switch (*target) {
					case '(':
						*target = ')';
						break;
					case ')':
						*target = '(';
						break;
					default:
						break;
				}
				target--;
			}
			block_type = _HEB_BLOCK_TYPE_ENG;
		} else {
			while(!isheb(*(tmp+1)) && (int)*(tmp+1)!='\n' && block_end<str->value.str.len-1) {
				tmp++;
				block_end++;
				block_length++;
			}
			while ((_isblank((int)*tmp) || ispunct((int)*tmp)) && *tmp!='/' && *tmp!='-' && block_end>block_start) {
				tmp--;
				block_end--;
			}
			for (i=block_end; i>=block_start; i--) {
				*target = str->value.str.val[i];
				target--;
			}
			block_type = _HEB_BLOCK_TYPE_HEB;
		}
		block_start=block_end+1;
	} while(block_end<str->value.str.len-1);


	broken_str = (char *) emalloc(str->value.str.len+1);
	begin=end=str->value.str.len-1;
	target = broken_str;
		
	while (1) {
		char_count=0;
		while ((!max_chars || char_count<max_chars) && begin>0) {
			char_count++;
			begin--;
			if (begin<=0 || _isnewline(heb_str[begin])) {
				while(begin>0 && _isnewline(heb_str[begin-1])) {
					begin--;
					char_count++;
				}
				break;
			}
		}
		if (char_count==max_chars) { /* try to avoid breaking words */
			int new_char_count=char_count, new_begin=begin;
			
			while (new_char_count>0) {
				if (_isblank(heb_str[new_begin]) || _isnewline(heb_str[new_begin])) {
					break;
				}
				new_begin++;
				new_char_count--;
			}
			if (new_char_count>0) {
				char_count=new_char_count;
				begin=new_begin;
			}
		}
		orig_begin=begin;
		
		if (_isblank(heb_str[begin])) {
			heb_str[begin]='\n';
		}
		while (begin<=end && _isnewline(heb_str[begin])) { /* skip leading newlines */
			begin++;
		}
		for (i=begin; i<=end; i++) { /* copy content */
			*target = heb_str[i];
			target++;
		}
		for (i=orig_begin; i<=end && _isnewline(heb_str[i]); i++) {
			*target = heb_str[i];
			target++;
		}
		begin=orig_begin;

		if (begin<=0) {
			*target = 0;
			break;
		}
		begin--;
		end=begin;
	}
	efree(heb_str);

	if (convert_newlines) {
		_php3_char_to_str(broken_str,str->value.str.len,'\n',"<br>\n",5,return_value);
		efree(broken_str);
	} else {
		return_value->value.str.val = broken_str;
		return_value->value.str.len = str->value.str.len;
		return_value->type = IS_STRING;
	}
}


/* {{{ proto string hebrev(string str [, int max_chars_per_line])
   Convert logical Hebrew text to visual text */
void php3_hebrev(INTERNAL_FUNCTION_PARAMETERS)
{
	_php3_hebrev(INTERNAL_FUNCTION_PARAM_PASSTHRU,0);
}
/* }}} */

/* {{{ proto string hebrev(string str [, int max_chars_per_line])
   Convert logical Hebrew text to visual text with newline conversion */
void php3_hebrev_with_conversion(INTERNAL_FUNCTION_PARAMETERS)
{
	_php3_hebrev(INTERNAL_FUNCTION_PARAM_PASSTHRU,1);
}
/* }}} */

/* {{{ proto string nl2br(string str)
   Converts newlines to HTML line breaks */
void php3_newline_to_br(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *str;
	
	if (ARG_COUNT(ht)!=1 || getParameters(ht, 1, &str)==FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	convert_to_string(str);
	
	_php3_char_to_str(str->value.str.val,str->value.str.len,'\n',"<br>\n",5,return_value);
}
/* }}} */

/* {{{ proto string setlocale(string category, string locale)
   Set locale information */
void php3_setlocale(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *category, *locale;
	int cat;
	char *loc, *retval;

	if (ARG_COUNT(ht)!=2 || getParameters(ht, 2, &category, &locale)==FAILURE)
		WRONG_PARAM_COUNT;
#if HAVE_SETLOCALE
	convert_to_string(category);
	convert_to_string(locale);
	if (!strcasecmp ("LC_ALL", category->value.str.val))
		cat = LC_ALL;
	else if (!strcasecmp ("LC_COLLATE", category->value.str.val))
		cat = LC_COLLATE;
	else if (!strcasecmp ("LC_CTYPE", category->value.str.val))
		cat = LC_CTYPE;
	else if (!strcasecmp ("LC_MONETARY", category->value.str.val))
		cat = LC_MONETARY;
	else if (!strcasecmp ("LC_NUMERIC", category->value.str.val))
		cat = LC_NUMERIC;
	else if (!strcasecmp ("LC_TIME", category->value.str.val))
		cat = LC_TIME;
	else {
		php3_error(E_WARNING,"Invalid locale category name %s, must be one of LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY, LC_NUMERIC or LC_TIME", category->value.str.val);
		RETURN_FALSE;
	}
	if (!strcmp ("0", locale->value.str.val))
		loc = NULL;
	else
		loc = locale->value.str.val;
	retval = setlocale (cat, loc);
	if (retval) {
		RETVAL_STRING(retval,1);
		return;
	}
#endif
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto void parsestr(string encoded_string)
   Parses GET/POST/COOKIE data and sets global variables. */
void php3_parsestr(INTERNAL_FUNCTION_PARAMETERS)
{
	pval *arg;
	char *res = NULL;

	if (getParameters(ht, 1, &arg) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string(arg);
	if (arg->value.str.val && *arg->value.str.val) {
		res = estrndup(arg->value.str.val,arg->value.str.len);
	}
	php3_treat_data(PARSE_STRING, res);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
