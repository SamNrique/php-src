/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2004 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Wez Furlong <wez@php.net>                                    |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "pdo/php_pdo.h"
#include "pdo/php_pdo_driver.h"
#include "php_pdo_odbc.h"
#include "php_pdo_odbc_int.h"

#ifdef PHP_WIN32
# define PDO_ODBC_TYPE	"Win32"
#endif

#ifndef PDO_ODBC_TYPE
# warning Please fix configure to give your ODBC libraries a name
# define PDO_ODBC_TYPE	"Unknown"
#endif

/* {{{ pdo_odbc_functions[] */
function_entry pdo_odbc_functions[] = {
	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ pdo_odbc_module_entry */
zend_module_entry pdo_odbc_module_entry = {
	STANDARD_MODULE_HEADER,
	"PDO_ODBC",
	pdo_odbc_functions,
	PHP_MINIT(pdo_odbc),
	PHP_MSHUTDOWN(pdo_odbc),
	NULL,
	NULL,
	PHP_MINFO(pdo_odbc),
	"0.1",
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_PDO_ODBC
ZEND_GET_MODULE(pdo_odbc)
#endif

#ifdef SQL_ATTR_CONNECTION_POOLING
SQLUINTEGER pdo_odbc_pool_on = SQL_CP_OFF;
SQLUINTEGER pdo_odbc_pool_mode = SQL_CP_ONE_PER_HENV;
#endif

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(pdo_odbc)
{
#ifdef SQL_ATTR_CONNECTION_POOLING
	char *pooling_val = NULL;
#endif

	if (FAILURE == php_pdo_register_driver(&pdo_odbc_driver)) {
		return FAILURE;
	}

	pdo_odbc_init_error_table();

#ifdef SQL_ATTR_CONNECTION_POOLING
	/* ugh, we don't really .ini stuff in PDO, but since ODBC connection
	 * pooling is process wide, we can't set it from within the scope of a
	 * request without affecting others, which goes against our isolated request
	 * policy.  So, we use cfg_get_string here to check it this once.
	 * */
	if (FAILURE == cfg_get_string("pdo_odbc.connection_pooling", &pooling_val) || pooling_val == NULL) {
		pooling_val = "strict";
	}
	if (strcasecmp(pooling_val, "strict") == 0 || strcmp(pooling_val, "1") == 0) {
		pdo_odbc_pool_on = SQL_CP_ONE_PER_HENV;
		pdo_odbc_pool_mode = SQL_CP_STRICT_MATCH;
	} else if (strcasecmp(pooling_val, "relaxed") == 0) {
		pdo_odbc_pool_on = SQL_CP_ONE_PER_HENV;
		pdo_odbc_pool_mode = SQL_CP_RELAXED_MATCH;
	} else if (*pooling_val == '\0' || strcasecmp(pooling_val, "off") == 0) {
		pdo_odbc_pool_on = SQL_CP_OFF;
	} else {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Error in pdo_odbc.connection_pooling configuration.  Value MUST be one of 'strict', 'relaxed' or 'off'");
		return FAILURE;
	}

	if (pdo_odbc_pool_on != SQL_CP_OFF) {
		SQLSetEnvAttr(SQL_NULL_HANDLE, SQL_ATTR_CONNECTION_POOLING, (void*)pdo_odbc_pool_on, 0);
	}
#endif

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(pdo_odbc)
{
	php_pdo_unregister_driver(&pdo_odbc_driver);
	pdo_odbc_fini_error_table();
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(pdo_odbc)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "PDO Driver for ODBC (" PDO_ODBC_TYPE ")" , "enabled");
#ifdef SQL_ATTR_CONNECTION_POOLING
	php_info_print_table_row(2, "ODBC Connection Pooling",	pdo_odbc_pool_on == SQL_CP_OFF ?
			"Disabled" : (pdo_odbc_pool_mode == SQL_CP_STRICT_MATCH ? "Enabled, strict matching" : "Enabled, relaxed matching"));
#endif
	php_info_print_table_end();

}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
