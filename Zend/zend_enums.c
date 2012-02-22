/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) 1998-2011 Zend Technologies Ltd. (http://www.zend.com) |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        | 
   | available through the world-wide-web at the following url:           |
   | http://www.zend.com/license/2_00.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
   | Authors: Christian Seiler <chris_se@gmx.net>                         |
   |          Dmitry Stogov <dmitry@zend.com>                             |
   |          Marcus Boerger <helly@php.net>                              |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#include "zend.h"
#include "zend_API.h"
#include "zend_closures.h"
#include "zend_exceptions.h"
#include "zend_interfaces.h"
#include "zend_objects.h"
#include "zend_objects_API.h"
#include "zend_globals.h"

ZEND_BEGIN_ARG_INFO(arginfo_enum__void, 0)
ZEND_END_ARG_INFO()

/* {{{ proto public static array [Enum Class]::length()
       Returns the number of enums values */
ZEND_NAMED_FUNCTION(ZEND_MN(enum_length)) 
{
	long arg_value;
	zend_class_entry *ce;
	HashPosition pos;
	zval **const_value;
	long result = 0;
	
	if (zend_parse_parameters_none() == FAILURE) {
		RETVAL_NULL(); 
		return;
	}
	
	if(!EG(called_scope)) {
		RETVAL_NULL(); 
		return;
	}
	
	array_init(return_value);
	
	ce = EG(called_scope);
	zend_hash_internal_pointer_reset_ex(&ce->constants_table, &pos);
    while (zend_hash_get_current_data_ex(&ce->constants_table, (void **) &const_value, &pos) == SUCCESS) {
		++result;
		zend_hash_move_forward_ex(&ce->constants_table, &pos);
	}
	RETURN_LONG(result);
}
/* }}} */

static const zend_function_entry enum_functions[] = {
	ZEND_FENTRY(length, ZEND_MN(enum_length), arginfo_enum__void, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_FE_END
};

ZEND_API int zend_register_enum_builtin_functions(zend_class_entry *new_enum_entry TSRMLS_DC) /* {{{ */
{
	return zend_register_functions(new_enum_entry, enum_functions, &new_enum_entry->function_table, MODULE_PERSISTENT TSRMLS_CC);
}
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
