/*
   +----------------------------------------------------------------------+
   | PHP Version 5                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2011 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Rasmus Lerdorf <rasmus@php.net>                             |
   |          Mike Jackson <mhjack@tscnet.com>                            |
   |          Steven Lawrance <slawrance@technologist.com>                |
   |          Harrie Hazewinkel <harrie@lisanza.net>                      |
   |          Johann Hanne <jonny@nurfuerspam.de>                         |
   |          Boris Lytockin <lytboris@gmail.com>                         |
   +----------------------------------------------------------------------+
 */

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "main/php_network.h"
#include "ext/standard/info.h"
#include "php_snmp.h"

#include "zend_exceptions.h"

#if HAVE_SPL
#include "ext/spl/spl_exceptions.h"
#endif

#if HAVE_SNMP

#include <sys/types.h>
#ifdef PHP_WIN32
#include <winsock2.h>
#include <errno.h>
#include <process.h>
#include "win32/time.h"
#elif defined(NETWARE)
#ifdef USE_WINSOCK
#include <novsock2.h>
#else
#include <sys/socket.h>
#endif
#include <errno.h>
#include <sys/timeval.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifndef _OSD_POSIX
#include <sys/errno.h>
#else
#include <errno.h>  /* BS2000/OSD uses <errno.h>, not <sys/errno.h> */
#endif
#include <netdb.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef __P
#ifdef __GNUC__
#define __P(args) args
#else
#define __P(args) ()
#endif
#endif

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

/* For net-snmp prior to 5.4 */
#ifndef HAVE_SHUTDOWN_SNMP_LOGGING
extern netsnmp_log_handler *logh_head;
#define shutdown_snmp_logging() \
	{ \
		snmp_disable_log(); \
		while(NULL != logh_head) \
			netsnmp_remove_loghandler( logh_head ); \
	}
#endif

#define SNMP_VALUE_LIBRARY	(0 << 0)
#define SNMP_VALUE_PLAIN	(1 << 0)
#define SNMP_VALUE_OBJECT	(1 << 1)

typedef struct snmp_session php_snmp_session;
#define PHP_SNMP_SESSION_RES_NAME "SNMP session"

#define PHP_SNMP_ADD_PROPERTIES(a, b) \
{ \
	int i = 0; \
	while (b[i].name != NULL) { \
		php_snmp_add_property((a), (b)[i].name, (b)[i].name_length, \
							(php_snmp_read_t)(b)[i].read_func, (php_snmp_write_t)(b)[i].write_func TSRMLS_CC); \
		i++; \
	} \
}

#define PHP_SNMP_ERRNO_NOERROR			0
#define PHP_SNMP_ERRNO_GENERIC			(1 << 1)
#define PHP_SNMP_ERRNO_TIMEOUT			(1 << 2)
#define PHP_SNMP_ERRNO_ERROR_IN_REPLY		(1 << 3)
#define PHP_SNMP_ERRNO_OID_NOT_INCREASING	(1 << 4)
#define PHP_SNMP_ERRNO_OID_PARSING_ERROR	(1 << 5)
#define PHP_SNMP_ERRNO_MULTIPLE_SET_QUERIES	(1 << 6)
#define PHP_SNMP_ERRNO_ANY	( \
		PHP_SNMP_ERRNO_GENERIC | \
		PHP_SNMP_ERRNO_TIMEOUT | \
		PHP_SNMP_ERRNO_ERROR_IN_REPLY | \
		PHP_SNMP_ERRNO_OID_NOT_INCREASING | \
		PHP_SNMP_ERRNO_OID_PARSING_ERROR | \
		PHP_SNMP_ERRNO_MULTIPLE_SET_QUERIES | \
		PHP_SNMP_ERRNO_NOERROR \
	)

ZEND_DECLARE_MODULE_GLOBALS(snmp)
static PHP_GINIT_FUNCTION(snmp);

/* constant - can be shared among threads */
static oid objid_mib[] = {1, 3, 6, 1, 2, 1};

static int le_snmp_session;

/* Handlers */
static zend_object_handlers php_snmp_object_handlers;

/* Class entries */
zend_class_entry *php_snmp_ce;
zend_class_entry *php_snmp_exception_ce;

/* Class object properties */
static HashTable php_snmp_properties;

/* {{{ arginfo */

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmpget, 0, 0, 3)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, community)
	ZEND_ARG_INFO(0, object_id)
	ZEND_ARG_INFO(0, timeout)
	ZEND_ARG_INFO(0, retries)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmpgetnext, 0, 0, 3)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, community)
	ZEND_ARG_INFO(0, object_id)
	ZEND_ARG_INFO(0, timeout)
	ZEND_ARG_INFO(0, retries)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmpwalk, 0, 0, 3)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, community)
	ZEND_ARG_INFO(0, object_id)
	ZEND_ARG_INFO(0, timeout)
	ZEND_ARG_INFO(0, retries)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmprealwalk, 0, 0, 3)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, community)
	ZEND_ARG_INFO(0, object_id)
	ZEND_ARG_INFO(0, timeout)
	ZEND_ARG_INFO(0, retries)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmpset, 0, 0, 5)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, community)
	ZEND_ARG_INFO(0, object_id)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, timeout)
	ZEND_ARG_INFO(0, retries)
ZEND_END_ARG_INFO()


ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp_get_quick_print, 0, 0, 1)
	ZEND_ARG_INFO(0, d)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp_set_quick_print, 0, 0, 1)
	ZEND_ARG_INFO(0, quick_print)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp_set_enum_print, 0, 0, 1)
	ZEND_ARG_INFO(0, enum_print)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp_set_oid_output_format, 0, 0, 1)
	ZEND_ARG_INFO(0, oid_format)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp2_get, 0, 0, 3)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, community)
	ZEND_ARG_INFO(0, object_id)
	ZEND_ARG_INFO(0, timeout)
	ZEND_ARG_INFO(0, retries)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp2_getnext, 0, 0, 3)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, community)
	ZEND_ARG_INFO(0, object_id)
	ZEND_ARG_INFO(0, timeout)
	ZEND_ARG_INFO(0, retries)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp2_walk, 0, 0, 3)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, community)
	ZEND_ARG_INFO(0, object_id)
	ZEND_ARG_INFO(0, timeout)
	ZEND_ARG_INFO(0, retries)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp2_real_walk, 0, 0, 3)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, community)
	ZEND_ARG_INFO(0, object_id)
	ZEND_ARG_INFO(0, timeout)
	ZEND_ARG_INFO(0, retries)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp2_set, 0, 0, 5)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, community)
	ZEND_ARG_INFO(0, object_id)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, timeout)
	ZEND_ARG_INFO(0, retries)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp3_get, 0, 0, 8)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, sec_name)
	ZEND_ARG_INFO(0, sec_level)
	ZEND_ARG_INFO(0, auth_protocol)
	ZEND_ARG_INFO(0, auth_passphrase)
	ZEND_ARG_INFO(0, priv_protocol)
	ZEND_ARG_INFO(0, priv_passphrase)
	ZEND_ARG_INFO(0, object_id)
	ZEND_ARG_INFO(0, timeout)
	ZEND_ARG_INFO(0, retries)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp3_getnext, 0, 0, 8)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, sec_name)
	ZEND_ARG_INFO(0, sec_level)
	ZEND_ARG_INFO(0, auth_protocol)
	ZEND_ARG_INFO(0, auth_passphrase)
	ZEND_ARG_INFO(0, priv_protocol)
	ZEND_ARG_INFO(0, priv_passphrase)
	ZEND_ARG_INFO(0, object_id)
	ZEND_ARG_INFO(0, timeout)
	ZEND_ARG_INFO(0, retries)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp3_walk, 0, 0, 8)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, sec_name)
	ZEND_ARG_INFO(0, sec_level)
	ZEND_ARG_INFO(0, auth_protocol)
	ZEND_ARG_INFO(0, auth_passphrase)
	ZEND_ARG_INFO(0, priv_protocol)
	ZEND_ARG_INFO(0, priv_passphrase)
	ZEND_ARG_INFO(0, object_id)
	ZEND_ARG_INFO(0, timeout)
	ZEND_ARG_INFO(0, retries)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp3_real_walk, 0, 0, 8)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, sec_name)
	ZEND_ARG_INFO(0, sec_level)
	ZEND_ARG_INFO(0, auth_protocol)
	ZEND_ARG_INFO(0, auth_passphrase)
	ZEND_ARG_INFO(0, priv_protocol)
	ZEND_ARG_INFO(0, priv_passphrase)
	ZEND_ARG_INFO(0, object_id)
	ZEND_ARG_INFO(0, timeout)
	ZEND_ARG_INFO(0, retries)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp3_set, 0, 0, 10)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, sec_name)
	ZEND_ARG_INFO(0, sec_level)
	ZEND_ARG_INFO(0, auth_protocol)
	ZEND_ARG_INFO(0, auth_passphrase)
	ZEND_ARG_INFO(0, priv_protocol)
	ZEND_ARG_INFO(0, priv_passphrase)
	ZEND_ARG_INFO(0, object_id)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, timeout)
	ZEND_ARG_INFO(0, retries)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp_set_valueretrieval, 0, 0, 1)
	ZEND_ARG_INFO(0, method)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_snmp_get_valueretrieval, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp_read_mib, 0, 0, 1)
	ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()

/* OO arginfo */

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp_create, 0, 0, 3)
	ZEND_ARG_INFO(0, version)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, community)
	ZEND_ARG_INFO(0, timeout)
	ZEND_ARG_INFO(0, retries)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_snmp_void, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp_setSecurity, 0, 0, 8)
	ZEND_ARG_INFO(0, session)
	ZEND_ARG_INFO(0, sec_level)
	ZEND_ARG_INFO(0, auth_protocol)
	ZEND_ARG_INFO(0, auth_passphrase)
	ZEND_ARG_INFO(0, priv_protocol)
	ZEND_ARG_INFO(0, priv_passphrase)
	ZEND_ARG_INFO(0, contextName)
	ZEND_ARG_INFO(0, contextEngineID)
	ZEND_ARG_INFO(0, )
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp_get, 0, 0, 1)
	ZEND_ARG_INFO(0, object_id)
	ZEND_ARG_INFO(0, use_orignames)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp_walk, 0, 0, 4)
	ZEND_ARG_INFO(0, object_id)
	ZEND_ARG_INFO(0, suffix_keys)
	ZEND_ARG_INFO(0, max_repetitions)
	ZEND_ARG_INFO(0, non_repeaters)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp_set, 0, 0, 3)
	ZEND_ARG_INFO(0, object_id)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_snmp_class_set_quick_print, 0, 0, 1)
	ZEND_ARG_INFO(0, quick_print)
ZEND_END_ARG_INFO()
/* }}} */

struct objid_query {
	int count;
	int offset;
	int step;
	long non_repeaters;
	long max_repetitions;
	int valueretrieval;
	int array_output;
	int oid_increasing_check;
	snmpobjarg *vars;
};

/* {{{ snmp_functions[]
 */
const zend_function_entry snmp_functions[] = {
	PHP_FE(snmpget,					arginfo_snmpget)
	PHP_FE(snmpgetnext, 				arginfo_snmpgetnext)
	PHP_FE(snmpwalk, 				arginfo_snmpwalk)
	PHP_FE(snmprealwalk, 				arginfo_snmprealwalk)
	PHP_FALIAS(snmpwalkoid, snmprealwalk, 		arginfo_snmprealwalk)
	PHP_FE(snmpset, 				arginfo_snmpset)
	PHP_FE(snmp_get_quick_print, 			arginfo_snmp_get_quick_print)
	PHP_FE(snmp_set_quick_print, 			arginfo_snmp_set_quick_print)
	PHP_FE(snmp_set_enum_print, 			arginfo_snmp_set_enum_print)
	PHP_FE(snmp_set_oid_output_format, 		arginfo_snmp_set_oid_output_format)
	PHP_FALIAS(snmp_set_oid_numeric_print, snmp_set_oid_output_format, arginfo_snmp_set_oid_output_format)

	PHP_FE(snmp2_get, 				arginfo_snmp2_get)
	PHP_FE(snmp2_getnext, 				arginfo_snmp2_getnext)
	PHP_FE(snmp2_walk, 				arginfo_snmp2_walk)
	PHP_FE(snmp2_real_walk, 			arginfo_snmp2_real_walk)
	PHP_FE(snmp2_set, 				arginfo_snmp2_set)

	PHP_FE(snmp3_get, 				arginfo_snmp3_get)
	PHP_FE(snmp3_getnext, 				arginfo_snmp3_getnext)
	PHP_FE(snmp3_walk, 				arginfo_snmp3_walk)
	PHP_FE(snmp3_real_walk, 			arginfo_snmp3_real_walk)
	PHP_FE(snmp3_set, 				arginfo_snmp3_set)
	PHP_FE(snmp_set_valueretrieval,			arginfo_snmp_set_valueretrieval)
	PHP_FE(snmp_get_valueretrieval,			arginfo_snmp_get_valueretrieval)

	PHP_FE(snmp_read_mib, 				arginfo_snmp_read_mib)
	PHP_FE_END
};
/* }}} */

/* query an agent with GET method */
#define SNMP_CMD_GET		(1<<0)
/* query an agent with GETNEXT method */
#define SNMP_CMD_GETNEXT	(1<<1)
/* query an agent with SET method */
#define SNMP_CMD_SET		(1<<2)
/* walk the mib */
#define SNMP_CMD_WALK		(1<<3)
/* force values-only output */
#define SNMP_NUMERIC_KEYS	(1<<7)
/* use user-supplied OID names for keys in array output mode in GET method */
#define SNMP_ORIGINAL_NAMES_AS_KEYS	(1<<8)
/* use OID suffix (`index') for keys in array output mode in WALK  method */
#define SNMP_USE_SUFFIX_AS_KEYS	(1<<9)

#ifdef COMPILE_DL_SNMP
ZEND_GET_MODULE(snmp)
#endif

/* THREAD_LS snmp_module php_snmp_module; - may need one of these at some point */

/* {{{ PHP_GINIT_FUNCTION
 */
static PHP_GINIT_FUNCTION(snmp)
{
	snmp_globals->valueretrieval = SNMP_VALUE_LIBRARY;
}
/* }}} */

#define PHP_SNMP_SESSION_FREE(a) { \
	if ((*session)->a) { \
		efree((*session)->a); \
		(*session)->a = NULL; \
	} \
}

static void netsnmp_session_free(php_snmp_session **session)
{
	if (*session) {
		PHP_SNMP_SESSION_FREE(peername);
		PHP_SNMP_SESSION_FREE(community);
		PHP_SNMP_SESSION_FREE(securityName);
		PHP_SNMP_SESSION_FREE(contextEngineID);
		efree(*session);
		*session = NULL;
	}
}

static void php_snmp_session_destructor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	php_snmp_session *session = (php_snmp_session *)rsrc->ptr;
	netsnmp_session_free(&session);
}

static void php_snmp_object_free_storage(void *object TSRMLS_DC)
{
	php_snmp_object *intern = (php_snmp_object *)object;
	
	if (!intern) {
		return;
	}

	netsnmp_session_free(&(intern->session));

	zend_object_std_dtor(&intern->zo TSRMLS_CC);
	
	efree(intern);
}

static zend_object_value php_snmp_object_new(zend_class_entry *class_type TSRMLS_DC) /* {{{ */
{
	zend_object_value retval;
	php_snmp_object *intern;

	/* Allocate memory for it */
	intern = emalloc(sizeof(php_snmp_object));
	memset(&intern->zo, 0, sizeof(php_snmp_object));

	zend_object_std_init(&intern->zo, class_type TSRMLS_CC);
	object_properties_init(&intern->zo, class_type);

	retval.handle = zend_objects_store_put(intern, (zend_objects_store_dtor_t)zend_objects_destroy_object, (zend_objects_free_object_storage_t) php_snmp_object_free_storage, NULL TSRMLS_CC);
	retval.handlers = (zend_object_handlers *) &php_snmp_object_handlers;

	return retval;
	
}

/* {{{ php_snmp_error
 *
 * Record last SNMP-related error in object
 *
 */
static void php_snmp_error(zval *object, const char *docref TSRMLS_DC, int type, const char *format, ...)
{
	va_list args;
	php_snmp_object *snmp_object;

	if (object) {
		snmp_object = (php_snmp_object *)zend_object_store_get_object(object TSRMLS_CC);
		if (type == PHP_SNMP_ERRNO_NOERROR) {
			memset(snmp_object->snmp_errstr, 0, sizeof(snmp_object->snmp_errstr));
		} else {
			va_start(args, format);
			vsnprintf(snmp_object->snmp_errstr, sizeof(snmp_object->snmp_errstr) - 1, format, args);
			va_end(args);
		}
		snmp_object->snmp_errno = type;
	}

	if (type == PHP_SNMP_ERRNO_NOERROR) {
		return;
	}

	if (object && (snmp_object->exceptions_enabled & type)) {
		zend_throw_exception_ex(php_snmp_exception_ce, type, snmp_object->snmp_errstr TSRMLS_CC);
	} else {
		va_start(args, format);
		php_verror(docref, "", E_WARNING, format, args TSRMLS_CC);
		va_end(args);
	}
}

/* }}} */

/* {{{ php_snmp_getvalue
*
* SNMP value to zval converter
*
*/
static void php_snmp_getvalue(struct variable_list *vars, zval *snmpval TSRMLS_DC, int valueretrieval)
{
	zval *val;
#ifdef BUGGY_SNMPRINT_VALUE
	char sbuf[2048];
#else
	char sbuf[64];
#endif
	char *buf = &(sbuf[0]);
	char *dbuf = (char *)NULL;
	int buflen = sizeof(sbuf) - 1;
	int val_len = vars->val_len;
	
	if ((valueretrieval & SNMP_VALUE_PLAIN) == 0) {
		val_len += 32; /* snprint_value will add type info into value, make some space for it */
	}

	/* use emalloc() for large values, use static array otherwize */
	if(val_len > buflen){
		if ((dbuf = (char *)emalloc(val_len + 1))) {
			buf = dbuf;
			buflen = val_len;
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "emalloc() failed: %s, fallback to static array", strerror(errno));
		}
	}

	*buf = 0;

	MAKE_STD_ZVAL(val);

	if (valueretrieval & SNMP_VALUE_PLAIN) {
		switch (vars->type) {
		case ASN_BIT_STR:		/* 0x03, asn1.h */
			ZVAL_STRINGL(val, (char *)vars->val.bitstring, vars->val_len, 1);
			break;

		case ASN_OCTET_STR:		/* 0x04, asn1.h */
		case ASN_OPAQUE:		/* 0x44, snmp_impl.h */
			ZVAL_STRINGL(val, (char *)vars->val.string, vars->val_len, 1);
			break;

		case ASN_NULL:			/* 0x05, asn1.h */
			ZVAL_NULL(val);
			break;

		case ASN_OBJECT_ID:		/* 0x06, asn1.h */
			snprint_objid(buf, buflen, vars->val.objid, vars->val_len / sizeof(oid));
			ZVAL_STRING(val, buf, 1);
			break;

		case ASN_IPADDRESS:		/* 0x40, snmp_impl.h */
			snprintf(buf, buflen, "%d.%d.%d.%d",
				 (vars->val.string)[0], (vars->val.string)[1],
				 (vars->val.string)[2], (vars->val.string)[3]);
			buf[buflen]=0;
			ZVAL_STRING(val, buf, 1);
			break;

		case ASN_COUNTER:		/* 0x41, snmp_impl.h */
		case ASN_GAUGE:			/* 0x42, snmp_impl.h */
		/* ASN_UNSIGNED is the same as ASN_GAUGE */
		case ASN_TIMETICKS:		/* 0x43, snmp_impl.h */
		case ASN_UINTEGER:		/* 0x47, snmp_impl.h */
			snprintf(buf, buflen, "%lu", *vars->val.integer);
			buf[buflen]=0;
			ZVAL_STRING(val, buf, 1);
			break;

		case ASN_INTEGER:		/* 0x02, asn1.h */
			snprintf(buf, buflen, "%ld", *vars->val.integer);
			buf[buflen]=0;
			ZVAL_STRING(val, buf, 1);
			break;

#if defined(NETSNMP_WITH_OPAQUE_SPECIAL_TYPES) || defined(OPAQUE_SPECIAL_TYPES)
		case ASN_OPAQUE_FLOAT:		/* 0x78, asn1.h */
			snprintf(buf, buflen, "%f", *vars->val.floatVal);
			ZVAL_STRING(val, buf, 1);
			break;

		case ASN_OPAQUE_DOUBLE:		/* 0x79, asn1.h */
			snprintf(buf, buflen, "%Lf", *vars->val.doubleVal);
			ZVAL_STRING(val, buf, 1);
			break;

		case ASN_OPAQUE_I64:		/* 0x80, asn1.h */
			printI64(buf, vars->val.counter64);
			ZVAL_STRING(val, buf, 1);
			break;

		case ASN_OPAQUE_U64:		/* 0x81, asn1.h */
#endif
		case ASN_COUNTER64:		/* 0x46, snmp_impl.h */
			printU64(buf, vars->val.counter64);
			ZVAL_STRING(val, buf, 1);
			break;

		default:
			ZVAL_STRING(val, "Unknown value type", 1);
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown value type: %u", vars->type);
			break;
		}
	} else /* use Net-SNMP value translation */ {
		snprint_value(buf, buflen, vars->name, vars->name_length, vars);
		ZVAL_STRING(val, buf, 1);
	}

	if (valueretrieval & SNMP_VALUE_OBJECT) {
		object_init(snmpval);
		add_property_long(snmpval, "type", vars->type);
		add_property_zval(snmpval, "value", val);
	} else  {
		*snmpval = *val;
		zval_copy_ctor(snmpval);
	}
	zval_ptr_dtor(&val);

	if(dbuf){ /* malloc was used to store value */
		efree(dbuf);
	}
}
/* }}} */

/* {{{ php_snmp_internal
*
* SNMP object fetcher/setter for all SNMP versions
*
*/
static void php_snmp_internal(INTERNAL_FUNCTION_PARAMETERS, int st, 
							struct snmp_session *session,
							struct objid_query *objid_query)
{
	struct snmp_session *ss;
	struct snmp_pdu *pdu=NULL, *response;
	struct variable_list *vars;
	oid root[MAX_NAME_LEN];
	size_t rootlen = 0;
	int status, count, found;
	char buf[2048];
	char buf2[2048];
	int keepwalking=1;
	char *err;
	zval *snmpval = NULL;
	int snmp_errno;

	/* we start with retval=FALSE. If any actual data is aquired, retval will be set to appropriate type */
	RETVAL_FALSE;
	
	/* reset errno and errstr */
	php_snmp_error(getThis(), NULL TSRMLS_CC, PHP_SNMP_ERRNO_NOERROR, "");

	if (st & SNMP_CMD_WALK) { /* remember root OID */
		memmove((char *)root, (char *)(objid_query->vars[0].name), (objid_query->vars[0].name_length) * sizeof(oid));
		rootlen = objid_query->vars[0].name_length;
		objid_query->offset = objid_query->count;
	}

	if ((ss = snmp_open(session)) == NULL) {
		snmp_error(session, NULL, NULL, &err);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not open snmp connection: %s", err);
		free(err);
		RETVAL_FALSE;
		return;
	}

	if ((st & SNMP_CMD_SET) && objid_query->count > objid_query->step) {
		php_snmp_error(getThis(), NULL TSRMLS_CC, PHP_SNMP_ERRNO_MULTIPLE_SET_QUERIES, "Can not fit all OIDs for SET query into one packet, using multiple queries");
	}

	while (keepwalking) {
		keepwalking = 0;
		if (st & SNMP_CMD_WALK) {
			if (session->version == SNMP_VERSION_1) {
				pdu = snmp_pdu_create(SNMP_MSG_GETNEXT);
			} else {
				pdu = snmp_pdu_create(SNMP_MSG_GETBULK);
				pdu->non_repeaters = objid_query->non_repeaters;
				pdu->max_repetitions = objid_query->max_repetitions;
			}
			snmp_add_null_var(pdu, objid_query->vars[0].name, objid_query->vars[0].name_length);
		} else {
			if (st & SNMP_CMD_GET) {
				pdu = snmp_pdu_create(SNMP_MSG_GET);
			} else if (st & SNMP_CMD_GETNEXT) {
				pdu = snmp_pdu_create(SNMP_MSG_GETNEXT);
			} else if (st & SNMP_CMD_SET) {
				pdu = snmp_pdu_create(SNMP_MSG_SET);
			} else {
				snmp_close(ss);
				php_error_docref(NULL TSRMLS_CC, E_ERROR, "Unknown SNMP command (internals)");
				RETVAL_FALSE;
				return;
			}
			for (count = 0; objid_query->offset < objid_query->count && count < objid_query->step; objid_query->offset++, count++){
				if (st & SNMP_CMD_SET) {
					if ((snmp_errno = snmp_add_var(pdu, objid_query->vars[objid_query->offset].name, objid_query->vars[objid_query->offset].name_length, objid_query->vars[objid_query->offset].type, objid_query->vars[objid_query->offset].value))) {
						snprint_objid(buf, sizeof(buf), objid_query->vars[objid_query->offset].name, objid_query->vars[objid_query->offset].name_length);
						php_snmp_error(getThis(), NULL TSRMLS_CC, PHP_SNMP_ERRNO_OID_PARSING_ERROR, "Could not add variable: OID='%s' type='%c' value='%s': %s", buf, objid_query->vars[objid_query->offset].type, objid_query->vars[objid_query->offset].value, snmp_api_errstring(snmp_errno));
						snmp_free_pdu(pdu);
						snmp_close(ss);
						RETVAL_FALSE;
						return;
					}
				} else {
					snmp_add_null_var(pdu, objid_query->vars[objid_query->offset].name, objid_query->vars[objid_query->offset].name_length);
				}
			}
			if(pdu->variables == NULL){
				snmp_free_pdu(pdu);
				snmp_close(ss);
				RETVAL_FALSE;
				return;
			}
		}

retry:
		status = snmp_synch_response(ss, pdu, &response);
		if (status == STAT_SUCCESS) {
			if (response->errstat == SNMP_ERR_NOERROR) {
				if (st & SNMP_CMD_SET) {
					if (objid_query->offset < objid_query->count) { /* we have unprocessed OIDs */
						keepwalking = 1;
						continue;
					}
					snmp_free_pdu(response);
					snmp_close(ss);
					RETVAL_TRUE;
					return;
				}
				for (vars = response->variables; vars; vars = vars->next_variable) {
					/* do not output errors as values */
					if ( 	vars->type == SNMP_ENDOFMIBVIEW || 
						vars->type == SNMP_NOSUCHOBJECT || 
						vars->type == SNMP_NOSUCHINSTANCE ) {
						if ((st & SNMP_CMD_WALK) && Z_TYPE_P(return_value) == IS_ARRAY) {
							break;
						}
						snprint_objid(buf, sizeof(buf), vars->name, vars->name_length);
						snprint_value(buf2, sizeof(buf2), vars->name, vars->name_length, vars);
						php_snmp_error(getThis(), NULL TSRMLS_CC, PHP_SNMP_ERRNO_ERROR_IN_REPLY, "Error in packet at '%s': %s", buf, buf2);
						continue;
					}
					
					if ((st & SNMP_CMD_WALK) && 
						(vars->name_length < rootlen || memcmp(root, vars->name, rootlen * sizeof(oid)))) { /* not part of this subtree */
						if (Z_TYPE_P(return_value) == IS_ARRAY) { /* some records are fetched already, shut down further lookup */
							keepwalking = 0;
						} else {
							/* first fetched OID is out of subtree, fallback to GET query */
							st |= SNMP_CMD_GET;
							st ^= SNMP_CMD_WALK;
							objid_query->offset = 0;
							keepwalking = 1;
						}
						break;
					}

					MAKE_STD_ZVAL(snmpval);
					php_snmp_getvalue(vars, snmpval TSRMLS_CC, objid_query->valueretrieval);

					if (objid_query->array_output) {
						if (Z_TYPE_P(return_value) == IS_BOOL) {
							array_init(return_value);
						}
						if (st & SNMP_NUMERIC_KEYS) {
							add_next_index_zval(return_value, snmpval);
						} else if (st & SNMP_ORIGINAL_NAMES_AS_KEYS && st & SNMP_CMD_GET) {
							found = 0;
							for (count = 0; count < objid_query->count; count++) {
								if (objid_query->vars[count].name_length == vars->name_length && snmp_oid_compare(objid_query->vars[count].name, objid_query->vars[count].name_length, vars->name, vars->name_length) == 0) {
									found = 1;
									objid_query->vars[count].name_length = 0; /* mark this name as used */
									break;
								}
							}
							if (found) {
								add_assoc_zval(return_value, objid_query->vars[count].oid, snmpval);
							} else {
								snprint_objid(buf2, sizeof(buf2), vars->name, vars->name_length);
								php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not find original OID name for '%s'", buf2);
							}
						} else if (st & SNMP_USE_SUFFIX_AS_KEYS && st & SNMP_CMD_WALK) {
							snprint_objid(buf2, sizeof(buf2), vars->name, vars->name_length);
							if (objid_query->vars[0].name_length <= vars->name_length && snmp_oid_compare(objid_query->vars[0].name, objid_query->vars[0].name_length, vars->name, objid_query->vars[0].name_length) == 0) {
								buf2[0] = '\0';
								count = objid_query->vars[0].name_length;
								while(count < vars->name_length){
									sprintf(buf, "%lu.", vars->name[count]);
									strcat(buf2, buf);
									count++;
								}
								buf2[strlen(buf2) - 1] = '\0'; /* remove trailing '.' */
							}
							add_assoc_zval(return_value, buf2, snmpval);
						} else {
							snprint_objid(buf2, sizeof(buf2), vars->name, vars->name_length);
							add_assoc_zval(return_value, buf2, snmpval);
						}
					} else {
						*return_value = *snmpval;
						zval_copy_ctor(return_value);
						zval_ptr_dtor(&snmpval);
						break;
					}

					/* OID increase check */
					if (st & SNMP_CMD_WALK) {
						if (objid_query->oid_increasing_check == TRUE && snmp_oid_compare(objid_query->vars[0].name, objid_query->vars[0].name_length, vars->name, vars->name_length) >= 0) {
							snprint_objid(buf2, sizeof(buf2), vars->name, vars->name_length);
							php_snmp_error(getThis(), NULL TSRMLS_CC, PHP_SNMP_ERRNO_OID_NOT_INCREASING, "Error: OID not increasing: %s", buf2);
							keepwalking = 0;
						} else {
							memmove((char *)(objid_query->vars[0].name), (char *)vars->name, vars->name_length * sizeof(oid));
							objid_query->vars[0].name_length = vars->name_length;
							keepwalking = 1;
						}
					}
				}
				if (objid_query->offset < objid_query->count) { /* we have unprocessed OIDs */
					keepwalking = 1;
				}
			} else {
				if (!(st & SNMP_CMD_WALK) || response->errstat != SNMP_ERR_NOSUCHNAME || Z_TYPE_P(return_value) == IS_BOOL) {
					for (	count=1, vars = response->variables;
						vars && count != response->errindex;
						vars = vars->next_variable, count++);

					if (st & (SNMP_CMD_GET | SNMP_CMD_GETNEXT) && response->errstat == SNMP_ERR_TOOBIG && objid_query->step > 1) { /* Answer will not fit into single packet */
						objid_query->offset = ((objid_query->offset > objid_query->step) ? (objid_query->offset - objid_query->step) : 0 );
						objid_query->step /= 2;
						snmp_free_pdu(response);
						keepwalking = 1;
						continue;
					}
					if (vars) {
						snprint_objid(buf, sizeof(buf), vars->name, vars->name_length);
						php_snmp_error(getThis(), NULL TSRMLS_CC, PHP_SNMP_ERRNO_ERROR_IN_REPLY, "Error in packet at '%s': %s", buf, snmp_errstring(response->errstat));
					} else {
						php_snmp_error(getThis(), NULL TSRMLS_CC, PHP_SNMP_ERRNO_ERROR_IN_REPLY, "Error in packet at %u object_id: %s", response->errindex, snmp_errstring(response->errstat));
					}
					if (st & (SNMP_CMD_GET | SNMP_CMD_GETNEXT)) { /* cut out bogus OID and retry */
						if ((pdu = snmp_fix_pdu(response, ((st & SNMP_CMD_GET) ? SNMP_MSG_GET : SNMP_MSG_GETNEXT) )) != NULL) {
							snmp_free_pdu(response);
							goto retry;
						}
					}
					snmp_free_pdu(response);
					snmp_close(ss);
					if (objid_query->array_output) {
						zval_dtor(return_value);
					}
					RETVAL_FALSE;
					return;
				}
			}
		} else if (status == STAT_TIMEOUT) {
			php_snmp_error(getThis(), NULL TSRMLS_CC, PHP_SNMP_ERRNO_TIMEOUT, "No response from %s", session->peername);
			if (objid_query->array_output) {
				zval_dtor(return_value);
			}
			snmp_close(ss);
			RETVAL_FALSE;
			return;
		} else {    /* status == STAT_ERROR */
			snmp_error(ss, NULL, NULL, &err);
			php_snmp_error(getThis(), NULL TSRMLS_CC, PHP_SNMP_ERRNO_GENERIC, "Fatal error: %s", err);
			free(err);
			if (objid_query->array_output) {
				zval_dtor(return_value);
			}
			snmp_close(ss);
			RETVAL_FALSE;
			return;
		}
		if (response) {
			snmp_free_pdu(response);
		}
	} /* keepwalking */
	snmp_close(ss);
}
/* }}} */

/* {{{ php_snmp_parse_oid
*
* OID parser (and type, value for SNMP_SET command)
*/

static int php_snmp_parse_oid(zval *object, int st, struct objid_query *objid_query, zval **oid, zval **type, zval **value TSRMLS_DC)
{
	char *pptr;
	HashPosition pos_oid, pos_type, pos_value;
	zval **tmp_oid, **tmp_type, **tmp_value;

	if (Z_TYPE_PP(oid) != IS_ARRAY) {
		if (Z_ISREF_PP(oid)) {
			SEPARATE_ZVAL(oid);
		}
		convert_to_string_ex(oid);
	} else if (Z_TYPE_PP(oid) == IS_ARRAY) {
		zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(oid), &pos_oid);
	}

	if (st & SNMP_CMD_SET) {
		if (Z_TYPE_PP(type) != IS_ARRAY) {
			if (Z_ISREF_PP(type)) {
				SEPARATE_ZVAL(type);
			}
			convert_to_string_ex(type);
		} else if (Z_TYPE_PP(type) == IS_ARRAY) {
			zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(type), &pos_type);
		}

		if (Z_TYPE_PP(value) != IS_ARRAY) {
			if (Z_ISREF_PP(value)) {
				SEPARATE_ZVAL(value);
			}
			convert_to_string_ex(value);
		} else if (Z_TYPE_PP(value) == IS_ARRAY) {
			zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(value), &pos_value);
		}
	}

	objid_query->count = 0;
	objid_query->array_output = ((st & SNMP_CMD_WALK) ? TRUE : FALSE);
	if (Z_TYPE_PP(oid) == IS_STRING) {
		objid_query->vars = (snmpobjarg *)emalloc(sizeof(snmpobjarg));
		if (objid_query->vars == NULL) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "emalloc() failed while parsing oid: %s", strerror(errno));
			efree(objid_query->vars);
			return FALSE;
		}
		objid_query->vars[objid_query->count].oid = Z_STRVAL_PP(oid);
		if (st & SNMP_CMD_SET) {
			if (Z_TYPE_PP(type) == IS_STRING && Z_TYPE_PP(value) == IS_STRING) {
				if (Z_STRLEN_PP(type) != 1) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "Bogus type '%s', should be single char, got %u", Z_STRVAL_PP(type), Z_STRLEN_PP(type));
					efree(objid_query->vars);
					return FALSE;
				}
				pptr = Z_STRVAL_PP(type);
				objid_query->vars[objid_query->count].type = *pptr;
				objid_query->vars[objid_query->count].value = Z_STRVAL_PP(value);
			} else {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Single objid and multiple type or values are not supported");
				efree(objid_query->vars);
				return FALSE;
			}
		}
		objid_query->count++;
	} else if (Z_TYPE_PP(oid) == IS_ARRAY) { /* we got objid array */
		if (zend_hash_num_elements(Z_ARRVAL_PP(oid)) == 0) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Got empty OID array");
			return FALSE;
		}
		objid_query->vars = (snmpobjarg *)emalloc(sizeof(snmpobjarg) * zend_hash_num_elements(Z_ARRVAL_PP(oid)));
		if (objid_query->vars == NULL) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "emalloc() failed while parsing oid array: %s", strerror(errno));
			efree(objid_query->vars);
			return FALSE;
		}
		objid_query->array_output = ( (st & SNMP_CMD_SET) ? FALSE : TRUE );
		for (	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(oid), &pos_oid);
			zend_hash_get_current_data_ex(Z_ARRVAL_PP(oid), (void **) &tmp_oid, &pos_oid) == SUCCESS;
			zend_hash_move_forward_ex(Z_ARRVAL_PP(oid), &pos_oid) ) {

			convert_to_string_ex(tmp_oid);
			objid_query->vars[objid_query->count].oid = Z_STRVAL_PP(tmp_oid);
			if (st & SNMP_CMD_SET) {
				if (Z_TYPE_PP(type) == IS_STRING) {
					pptr = Z_STRVAL_PP(type);
					objid_query->vars[objid_query->count].type = *pptr;
				} else if (Z_TYPE_PP(type) == IS_ARRAY) {
					if (SUCCESS == zend_hash_get_current_data_ex(Z_ARRVAL_PP(type), (void **) &tmp_type, &pos_type)) {
						convert_to_string_ex(tmp_type);
						if (Z_STRLEN_PP(tmp_type) != 1) {
							php_error_docref(NULL TSRMLS_CC, E_WARNING, "'%s': bogus type '%s', should be single char, got %u", Z_STRVAL_PP(tmp_oid), Z_STRVAL_PP(tmp_type), Z_STRLEN_PP(tmp_type));
							efree(objid_query->vars);
							return FALSE;
						}
						pptr = Z_STRVAL_PP(tmp_type);
						objid_query->vars[objid_query->count].type = *pptr;
						zend_hash_move_forward_ex(Z_ARRVAL_PP(type), &pos_type);
					} else {
						php_error_docref(NULL TSRMLS_CC, E_WARNING, "'%s': no type set", Z_STRVAL_PP(tmp_oid));
						efree(objid_query->vars);
						return FALSE;
					}
				}

				if (Z_TYPE_PP(value) == IS_STRING) {
					objid_query->vars[objid_query->count].value = Z_STRVAL_PP(value);
				} else if (Z_TYPE_PP(value) == IS_ARRAY) {
					if (SUCCESS == zend_hash_get_current_data_ex(Z_ARRVAL_PP(value), (void **) &tmp_value, &pos_value)) {
						convert_to_string_ex(tmp_value);
						objid_query->vars[objid_query->count].value = Z_STRVAL_PP(tmp_value);
						zend_hash_move_forward_ex(Z_ARRVAL_PP(value), &pos_value);
					} else {
						php_error_docref(NULL TSRMLS_CC, E_WARNING, "'%s': no value set", Z_STRVAL_PP(tmp_oid));
						efree(objid_query->vars);
						return FALSE;
					}
				}
			}
			objid_query->count++;
		}
	}

	/* now parse all OIDs */
	if (st & SNMP_CMD_WALK) {
		if (objid_query->count > 1) {
			php_snmp_error(object, NULL TSRMLS_CC, PHP_SNMP_ERRNO_OID_PARSING_ERROR, "Multi OID walks are not supported!");
			efree(objid_query->vars);
			return FALSE;
		}
		objid_query->vars[0].name_length = MAX_NAME_LEN;
		if (strlen(objid_query->vars[0].oid)) { /* on a walk, an empty string means top of tree - no error */
			if (!snmp_parse_oid(objid_query->vars[0].oid, objid_query->vars[0].name, &(objid_query->vars[0].name_length))) {
				php_snmp_error(object, NULL TSRMLS_CC, PHP_SNMP_ERRNO_OID_PARSING_ERROR, "Invalid object identifier: %s", objid_query->vars[0].oid);
				efree(objid_query->vars);
				return FALSE;
			}
		} else { 
			memmove((char *)objid_query->vars[0].name, (char *)objid_mib, sizeof(objid_mib));
			objid_query->vars[0].name_length = sizeof(objid_mib) / sizeof(oid);
		}
	} else {
		for (objid_query->offset = 0; objid_query->offset < objid_query->count; objid_query->offset++) {
			objid_query->vars[objid_query->offset].name_length = MAX_OID_LEN;
			if (!snmp_parse_oid(objid_query->vars[objid_query->offset].oid, objid_query->vars[objid_query->offset].name, &(objid_query->vars[objid_query->offset].name_length))) {
				php_snmp_error(object, NULL TSRMLS_CC, PHP_SNMP_ERRNO_OID_PARSING_ERROR, "Invalid object identifier: %s", objid_query->vars[objid_query->offset].oid);
				efree(objid_query->vars);
				return FALSE;
			}
		}
	}
	objid_query->offset = 0;
	objid_query->step = objid_query->count;
	return (objid_query->count > 0);
}
/* }}} */

/* {{{ netsnmp_session_init
	allocates memory for session and session->peername, caller should free it manually using netsnmp_session_free() and efree()
*/
static int netsnmp_session_init(php_snmp_session **session_p, int version, char *hostname, char *community, int timeout, int retries TSRMLS_DC)
{
	php_snmp_session *session;
	char *pptr;
	char buf[MAX_NAME_LEN];
	int force_ipv6 = FALSE;
	int n;
	struct sockaddr **psal;
	struct sockaddr **res;

	*session_p = (php_snmp_session *)emalloc(sizeof(php_snmp_session));
	session = *session_p;
	if (session == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "emalloc() failed allocating session");
		return (-1);
	}
	memset(session, 0, sizeof(php_snmp_session));

	strlcpy(buf, hostname, sizeof(buf));

	snmp_sess_init(session);

	session->version = version;
	session->remote_port = SNMP_PORT;

	session->peername = emalloc(MAX_NAME_LEN);
	if (session->peername == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "emalloc() failed while copying hostname");
		return (-1);
	}
	*(session->peername) = '\0';

	/* Reading the hostname and its optional non-default port number */
	if (*hostname == '[') { /* IPv6 address */
		force_ipv6 = TRUE;
		hostname++;
		if ((pptr = strchr(hostname, ']'))) {
			if (pptr[1] == ':') {
				session->remote_port = atoi(pptr + 2);
			}
			*pptr = '\0';
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "mailformed IPv6 address, closing square bracket missing");
			return (-1);
		}
	} else { /* IPv4 address */
		if ((pptr = strchr(hostname, ':'))) {
			session->remote_port = atoi(pptr + 1);
			*pptr = '\0';
		}
	}

	/* since Net-SNMP library requires 'udp6:' prefix for all IPv6 addresses (in FQDN form too) we need to
	   perform possible name resolution before running any SNMP queries */
	if ((n = php_network_getaddresses(hostname, SOCK_DGRAM, &psal, NULL TSRMLS_CC)) == 0) { /* some resover error */
		/* warnings sent, bailing out */
		return (-1);
	}

	res = psal;
	while (n-- > 0) {
		pptr = session->peername;
#if HAVE_GETADDRINFO && HAVE_IPV6 && HAVE_INET_NTOP
		if (force_ipv6 && (*res)->sa_family != AF_INET6) {
			res++;
			continue;
		}
		if ((*res)->sa_family == AF_INET6) {
			strcpy(session->peername, "udp6:");
			pptr = session->peername + strlen(session->peername);
			inet_ntop((*res)->sa_family, &(((struct sockaddr_in6*)(*res))->sin6_addr), pptr, MAX_NAME_LEN);
		} else if ((*res)->sa_family == AF_INET) {
			inet_ntop((*res)->sa_family, &(((struct sockaddr_in*)(*res))->sin_addr), pptr, MAX_NAME_LEN);
		} else {
			res++;
			continue;
		}
#else
		if (res->sa_family != AF_INET) {
			res++;
			continue;
		}
		strcat(pptr, inet_ntoa(res));
#endif
		break;
	}

	if (strlen(session->peername) == 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown failure while resolving '%s'", buf);
		return (-1);
	}
	/* XXX FIXME
		There should be check for non-empty session->peername!
	*/

	php_network_freeaddresses(psal);

	if (version == SNMP_VERSION_3) {
		/* Setting the security name. */
		session->securityName = estrdup(community);
		session->securityNameLen = strlen(session->securityName);
	} else {
		session->authenticator = NULL;
		session->community = (u_char *)estrdup(community);
		session->community_len = strlen(community);
	}

	session->retries = retries;
	session->timeout = timeout;
	return (0);
}
/* }}} */

/* {{{ int netsnmp_session_set_sec_level(struct snmp_session *s, char *level)
   Set the security level in the snmpv3 session */
static int netsnmp_session_set_sec_level(struct snmp_session *s, char *level)
{
	if (!strcasecmp(level, "noAuthNoPriv") || !strcasecmp(level, "nanp")) {
		s->securityLevel = SNMP_SEC_LEVEL_NOAUTH;
	} else if (!strcasecmp(level, "authNoPriv") || !strcasecmp(level, "anp")) {
		s->securityLevel = SNMP_SEC_LEVEL_AUTHNOPRIV;
	} else if (!strcasecmp(level, "authPriv") || !strcasecmp(level, "ap")) {
		s->securityLevel = SNMP_SEC_LEVEL_AUTHPRIV;
	} else {
		return (-1);
	}
	return (0);
}
/* }}} */

/* {{{ int netsnmp_session_set_auth_protocol(struct snmp_session *s, char *prot)
   Set the authentication protocol in the snmpv3 session */
static int netsnmp_session_set_auth_protocol(struct snmp_session *s, char *prot TSRMLS_DC)
{
	if (!strcasecmp(prot, "MD5")) {
		s->securityAuthProto = usmHMACMD5AuthProtocol;
		s->securityAuthProtoLen = USM_AUTH_PROTO_MD5_LEN;
	} else if (!strcasecmp(prot, "SHA")) {
		s->securityAuthProto = usmHMACSHA1AuthProtocol;
		s->securityAuthProtoLen = USM_AUTH_PROTO_SHA_LEN;
	} else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown authentication protocol '%s'", prot);
		return (-1);
	}
	return (0);
}
/* }}} */

/* {{{ int netsnmp_session_set_sec_protocol(struct snmp_session *s, char *prot)
   Set the security protocol in the snmpv3 session */
static int netsnmp_session_set_sec_protocol(struct snmp_session *s, char *prot TSRMLS_DC)
{
	if (!strcasecmp(prot, "DES")) {
		s->securityPrivProto = usmDESPrivProtocol;
		s->securityPrivProtoLen = USM_PRIV_PROTO_DES_LEN;
#ifdef HAVE_AES
	} else if (!strcasecmp(prot, "AES128") || !strcasecmp(prot, "AES")) {
		s->securityPrivProto = usmAESPrivProtocol;
		s->securityPrivProtoLen = USM_PRIV_PROTO_AES_LEN;
#endif
	} else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown security protocol '%s'", prot);
		return (-1);
	}
	return (0);
}
/* }}} */

/* {{{ int netsnmp_session_gen_auth_key(struct snmp_session *s, char *pass)
   Make key from pass phrase in the snmpv3 session */
static int netsnmp_session_gen_auth_key(struct snmp_session *s, char *pass TSRMLS_DC)
{
	int snmp_errno;
	s->securityAuthKeyLen = USM_AUTH_KU_LEN;
	if ((snmp_errno = generate_Ku(s->securityAuthProto, s->securityAuthProtoLen,
			(u_char *) pass, strlen(pass),
			s->securityAuthKey, &(s->securityAuthKeyLen)))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Error generating a key for authentication pass phrase '%s': %s", pass, snmp_api_errstring(snmp_errno));
		return (-1);
	}
	return (0);
}
/* }}} */

/* {{{ int netsnmp_session_gen_sec_key(struct snmp_session *s, u_char *pass)
   Make key from pass phrase in the snmpv3 session */
static int netsnmp_session_gen_sec_key(struct snmp_session *s, char *pass TSRMLS_DC)
{
	int snmp_errno;

	s->securityPrivKeyLen = USM_PRIV_KU_LEN;
	if ((snmp_errno = generate_Ku(s->securityAuthProto, s->securityAuthProtoLen,
			(u_char *)pass, strlen(pass),
			s->securityPrivKey, &(s->securityPrivKeyLen)))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Error generating a key for privacy pass phrase '%s': %s", pass, snmp_api_errstring(snmp_errno));
		return (-2);
	}
	return (0);
}
/* }}} */

/* {{{ in netsnmp_session_set_contextEngineID(struct snmp_session *s, u_char * contextEngineID)
   Set context Engine Id in the snmpv3 session */
static int netsnmp_session_set_contextEngineID(struct snmp_session *s, char * contextEngineID TSRMLS_DC)
{
	size_t	ebuf_len = 32, eout_len = 0;
	u_char	*ebuf = (u_char *) emalloc(ebuf_len);

	if (ebuf == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "malloc failure setting contextEngineID");
		return (-1);
	}
	if (!snmp_hex_to_binary(&ebuf, &ebuf_len, &eout_len, 1, contextEngineID)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Bad engine ID value '%s'", contextEngineID);
		efree(ebuf);
		return (-1);
	}

	if (s->contextEngineID) {
		efree(s->contextEngineID);
	}

	s->contextEngineID = ebuf;
	s->contextEngineIDLen = eout_len;
	return (0);
}
/* }}} */

/* {{{ php_set_security(struct snmp_session *session, char *sec_level, char *auth_protocol, char *auth_passphrase, char *priv_protocol, char *priv_passphrase, char *contextName, char *contextEngineID)
   Set all snmpv3-related security options */
static int netsnmp_session_set_security(struct snmp_session *session, char *sec_level, char *auth_protocol, char *auth_passphrase, char *priv_protocol, char *priv_passphrase, char *contextName, char *contextEngineID TSRMLS_DC)
{

	/* Setting the security level. */
	if (netsnmp_session_set_sec_level(session, sec_level)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid security level '%s'", sec_level);
		return (-1);
	}

	if (session->securityLevel == SNMP_SEC_LEVEL_AUTHNOPRIV || session->securityLevel == SNMP_SEC_LEVEL_AUTHPRIV) {

		/* Setting the authentication protocol. */
		if (netsnmp_session_set_auth_protocol(session, auth_protocol TSRMLS_CC)) {
			/* Warning message sent already, just bail out */
			return (-1);
		}

		/* Setting the authentication passphrase. */
		if (netsnmp_session_gen_auth_key(session, auth_passphrase TSRMLS_CC)) {
			/* Warning message sent already, just bail out */
			return (-1);
		}

		if (session->securityLevel == SNMP_SEC_LEVEL_AUTHPRIV) {
			/* Setting the security protocol. */
			if (netsnmp_session_set_sec_protocol(session, priv_protocol TSRMLS_CC)) {
				/* Warning message sent already, just bail out */
				return (-1);
			}

			/* Setting the security protocol passphrase. */
			if (netsnmp_session_gen_sec_key(session, priv_passphrase TSRMLS_CC)) {
				/* Warning message sent already, just bail out */
				return (-1);
			}
		}
	}

	/* Setting contextName if specified */
	if (contextName) {
		session->contextName = contextName;
		session->contextNameLen = strlen(contextName);
	}

	/* Setting contextEngineIS if specified */
	if (contextEngineID && strlen(contextEngineID) && netsnmp_session_set_contextEngineID(session, contextEngineID TSRMLS_CC)) {
		/* Warning message sent already, just bail out */
		return (-1);
	}

	return (0);
}
/* }}} */

/* {{{ php_snmp
*
* Generic SNMP handler for all versions.
* This function makes use of the internal SNMP object fetcher.
* Used both in old (non-OO) and OO API
*
*/
static void php_snmp(INTERNAL_FUNCTION_PARAMETERS, int st, int version)
{
	zval **oid, **value, **type;
	char *a1, *a2, *a3, *a4, *a5, *a6, *a7;
	int a1_len, a2_len, a3_len, a4_len, a5_len, a6_len, a7_len;
	zend_bool use_orignames = 0, suffix_keys = 0;
	long timeout = SNMP_DEFAULT_TIMEOUT;
	long retries = SNMP_DEFAULT_RETRIES;
	int argc = ZEND_NUM_ARGS();
	struct objid_query objid_query;
	php_snmp_session *session;
	int session_less_mode = (getThis() == NULL);
	php_snmp_object *snmp_object;
	php_snmp_object glob_snmp_object;
	
	objid_query.max_repetitions = -1;
	objid_query.non_repeaters = 0;
	objid_query.valueretrieval = SNMP_G(valueretrieval);
	objid_query.oid_increasing_check = TRUE;

	if (session_less_mode) {
		if (version == SNMP_VERSION_3) {
			if (st & SNMP_CMD_SET) {
				if (zend_parse_parameters(argc TSRMLS_CC, "sssssssZZZ|ll", &a1, &a1_len, &a2, &a2_len, &a3, &a3_len,
					&a4, &a4_len, &a5, &a5_len, &a6, &a6_len, &a7, &a7_len, &oid, &type, &value, &timeout, &retries) == FAILURE) {
					RETURN_FALSE;
				}
			} else {
				/* SNMP_CMD_GET
				 * SNMP_CMD_GETNEXT
				 * SNMP_CMD_WALK
				 */
				if (zend_parse_parameters(argc TSRMLS_CC, "sssssssZ|ll", &a1, &a1_len, &a2, &a2_len, &a3, &a3_len,
					&a4, &a4_len, &a5, &a5_len, &a6, &a6_len, &a7, &a7_len, &oid, &timeout, &retries) == FAILURE) {
					RETURN_FALSE;
				}
			}
		} else {
			if (st & SNMP_CMD_SET) {
				if (zend_parse_parameters(argc TSRMLS_CC, "ssZZZ|ll", &a1, &a1_len, &a2, &a2_len, &oid, &type, &value, &timeout, &retries) == FAILURE) {
					RETURN_FALSE;
				}
			} else {
				/* SNMP_CMD_GET
				 * SNMP_CMD_GETNEXT
				 * SNMP_CMD_WALK
				 */
				if (zend_parse_parameters(argc TSRMLS_CC, "ssZ|ll", &a1, &a1_len, &a2, &a2_len, &oid, &timeout, &retries) == FAILURE) {
					RETURN_FALSE;
				}
			}
		}
	} else {
		if (st & SNMP_CMD_SET) {
			if (zend_parse_parameters(argc TSRMLS_CC, "ZZZ", &oid, &type, &value) == FAILURE) {
				RETURN_FALSE;
			}
		} else if (st & SNMP_CMD_WALK) {
			if (zend_parse_parameters(argc TSRMLS_CC, "Z|bll", &oid, &suffix_keys, &(objid_query.max_repetitions), &(objid_query.non_repeaters)) == FAILURE) {
				RETURN_FALSE;
			}
			if (suffix_keys) {
				st |= SNMP_USE_SUFFIX_AS_KEYS;
			}
		} else if (st & SNMP_CMD_GET) {
			if (zend_parse_parameters(argc TSRMLS_CC, "Z|b", &oid, &use_orignames) == FAILURE) {
				RETURN_FALSE;
			}
			if (use_orignames) {
				st |= SNMP_ORIGINAL_NAMES_AS_KEYS;
			}
		} else {
			/* SNMP_CMD_GETNEXT
			 */
			if (zend_parse_parameters(argc TSRMLS_CC, "Z", &oid) == FAILURE) {
				RETURN_FALSE;
			}
		}
	}

	if (!php_snmp_parse_oid(getThis(), st, &objid_query, oid, type, value TSRMLS_CC)) {
		RETURN_FALSE;
	}

	if (session_less_mode) {
		if (netsnmp_session_init(&session, version, a1, a2, timeout, retries TSRMLS_CC)) {
			efree(objid_query.vars);
			netsnmp_session_free(&session);
			RETURN_FALSE;
		}
		if (version == SNMP_VERSION_3 && netsnmp_session_set_security(session, a3, a4, a5, a6, a7, NULL, NULL TSRMLS_CC)) {
			efree(objid_query.vars);
			netsnmp_session_free(&session);
			/* Warning message sent already, just bail out */
			RETURN_FALSE;
		}
	} else {
		zval *object = getThis();
		snmp_object = (php_snmp_object *)zend_object_store_get_object(object TSRMLS_CC);
		session = snmp_object->session;
		if (!session) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid or uninitialized SNMP object");
			efree(objid_query.vars);
			RETURN_FALSE;
		}

		if (snmp_object->max_oids > 0) {
			objid_query.step = snmp_object->max_oids;
			if (objid_query.max_repetitions < 0) { /* unspecified in function call, use session-wise */
				objid_query.max_repetitions = snmp_object->max_oids;
			}
		}
		objid_query.oid_increasing_check = snmp_object->oid_increasing_check;
		objid_query.valueretrieval = snmp_object->valueretrieval;
		glob_snmp_object.enum_print = netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PRINT_NUMERIC_ENUM);
		netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PRINT_NUMERIC_ENUM, snmp_object->enum_print);
		glob_snmp_object.quick_print = netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_QUICK_PRINT);
		netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_QUICK_PRINT, snmp_object->quick_print);
		glob_snmp_object.oid_output_format = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT);
		netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT, snmp_object->oid_output_format);
	}

	if (objid_query.max_repetitions < 0) {
		objid_query.max_repetitions = 20; /* provide correct default value */
	}

	php_snmp_internal(INTERNAL_FUNCTION_PARAM_PASSTHRU, st, session, &objid_query);
	
	efree(objid_query.vars);

	if (session_less_mode) {
		netsnmp_session_free(&session);
	} else {
		netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PRINT_NUMERIC_ENUM, glob_snmp_object.enum_print);
		netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_QUICK_PRINT, glob_snmp_object.quick_print);
		netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT, glob_snmp_object.oid_output_format);
	}
}
/* }}} */

/* {{{ proto mixed snmpget(string host, string community, mixed object_id [, int timeout [, int retries]]) 
   Fetch a SNMP object */
PHP_FUNCTION(snmpget)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, SNMP_CMD_GET, SNMP_VERSION_1);
}
/* }}} */

/* {{{ proto mixed snmpgetnext(string host, string community, mixed object_id [, int timeout [, int retries]]) 
   Fetch a SNMP object */
PHP_FUNCTION(snmpgetnext)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, SNMP_CMD_GETNEXT, SNMP_VERSION_1);
}
/* }}} */

/* {{{ proto mixed snmpwalk(string host, string community, mixed object_id [, int timeout [, int retries]]) 
   Return all objects under the specified object id */
PHP_FUNCTION(snmpwalk)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, (SNMP_CMD_WALK | SNMP_NUMERIC_KEYS), SNMP_VERSION_1);
}
/* }}} */

/* {{{ proto mixed snmprealwalk(string host, string community, mixed object_id [, int timeout [, int retries]])
   Return all objects including their respective object id withing the specified one */
PHP_FUNCTION(snmprealwalk)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, SNMP_CMD_WALK, SNMP_VERSION_1);
}
/* }}} */

/* {{{ proto bool snmpset(string host, string community, mixed object_id, mixed type, mixed value [, int timeout [, int retries]]) 
   Set the value of a SNMP object */
PHP_FUNCTION(snmpset)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, SNMP_CMD_SET, SNMP_VERSION_1);
}
/* }}} */

/* {{{ proto bool snmp_get_quick_print(void)
   Return the current status of quick_print */
PHP_FUNCTION(snmp_get_quick_print)
{
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	RETURN_BOOL(netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_QUICK_PRINT));
}
/* }}} */

/* {{{ proto bool snmp_set_quick_print(int quick_print)
   Return all objects including their respective object id withing the specified one */
PHP_FUNCTION(snmp_set_quick_print)
{
	long a1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &a1) == FAILURE) {
		RETURN_FALSE;
	}

	netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_QUICK_PRINT, (int)a1);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool snmp_set_enum_print(int enum_print)
   Return all values that are enums with their enum value instead of the raw integer */
PHP_FUNCTION(snmp_set_enum_print)
{
	long a1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &a1) == FAILURE) {
		RETURN_FALSE;
	}

	netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PRINT_NUMERIC_ENUM, (int) a1);
	RETURN_TRUE;
} 
/* }}} */

/* {{{ proto bool snmp_set_oid_output_format(int oid_format)
   Set the OID output format. */
PHP_FUNCTION(snmp_set_oid_output_format)
{
	long a1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &a1) == FAILURE) {
		RETURN_FALSE;
	}

	switch((int) a1) {
		case NETSNMP_OID_OUTPUT_SUFFIX:
		case NETSNMP_OID_OUTPUT_MODULE:
		case NETSNMP_OID_OUTPUT_FULL:
		case NETSNMP_OID_OUTPUT_NUMERIC:
		case NETSNMP_OID_OUTPUT_UCD:
		case NETSNMP_OID_OUTPUT_NONE:
			netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT, a1);
			RETURN_TRUE;
			break;
		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown SNMP output print format '%d'", (int) a1);
			RETURN_FALSE;
			break;
	}
} 
/* }}} */

/* {{{ proto mixed snmp2_get(string host, string community, mixed object_id [, int timeout [, int retries]]) 
   Fetch a SNMP object */
PHP_FUNCTION(snmp2_get)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, SNMP_CMD_GET, SNMP_VERSION_2c);
}
/* }}} */

/* {{{ proto mixed snmp2_getnext(string host, string community, mixed object_id [, int timeout [, int retries]]) 
   Fetch a SNMP object */
PHP_FUNCTION(snmp2_getnext)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, SNMP_CMD_GETNEXT, SNMP_VERSION_2c);
}
/* }}} */

/* {{{ proto mixed snmp2_walk(string host, string community, mixed object_id [, int timeout [, int retries]]) 
   Return all objects under the specified object id */
PHP_FUNCTION(snmp2_walk)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, (SNMP_CMD_WALK | SNMP_NUMERIC_KEYS), SNMP_VERSION_2c);
}
/* }}} */

/* {{{ proto mixed snmp2_real_walk(string host, string community, mixed object_id [, int timeout [, int retries]])
   Return all objects including their respective object id withing the specified one */
PHP_FUNCTION(snmp2_real_walk)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, SNMP_CMD_WALK, SNMP_VERSION_2c);
}
/* }}} */

/* {{{ proto bool snmp2_set(string host, string community, mixed object_id, mixed type, mixed value [, int timeout [, int retries]]) 
   Set the value of a SNMP object */
PHP_FUNCTION(snmp2_set)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, SNMP_CMD_SET, SNMP_VERSION_2c);
}
/* }}} */

/* {{{ proto mixed snmp3_get(string host, string sec_name, string sec_level, string auth_protocol, string auth_passphrase, string priv_protocol, string priv_passphrase, mixed object_id [, int timeout [, int retries]])
   Fetch the value of a SNMP object */
PHP_FUNCTION(snmp3_get)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, SNMP_CMD_GET, SNMP_VERSION_3);
}
/* }}} */

/* {{{ proto mixed snmp3_getnext(string host, string sec_name, string sec_level, string auth_protocol, string auth_passphrase, string priv_protocol, string priv_passphrase, mixed object_id [, int timeout [, int retries]])
   Fetch the value of a SNMP object */
PHP_FUNCTION(snmp3_getnext)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, SNMP_CMD_GETNEXT, SNMP_VERSION_3);
}
/* }}} */

/* {{{ proto mixed snmp3_walk(string host, string sec_name, string sec_level, string auth_protocol, string auth_passphrase, string priv_protocol, string priv_passphrase, mixed object_id [, int timeout [, int retries]])
   Fetch the value of a SNMP object */
PHP_FUNCTION(snmp3_walk)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, (SNMP_CMD_WALK | SNMP_NUMERIC_KEYS), SNMP_VERSION_3);
}
/* }}} */

/* {{{ proto mixed snmp3_real_walk(string host, string sec_name, string sec_level, string auth_protocol, string auth_passphrase, string priv_protocol, string priv_passphrase, mixed object_id [, int timeout [, int retries]])
   Fetch the value of a SNMP object */
PHP_FUNCTION(snmp3_real_walk)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, SNMP_CMD_WALK, SNMP_VERSION_3);
}
/* }}} */

/* {{{ proto bool snmp3_set(string host, string sec_name, string sec_level, string auth_protocol, string auth_passphrase, string priv_protocol, string priv_passphrase, mixed object_id, mixed type, mixed value [, int timeout [, int retries]])
   Fetch the value of a SNMP object */
PHP_FUNCTION(snmp3_set)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, SNMP_CMD_SET, SNMP_VERSION_3);
}
/* }}} */

/* {{{ proto bool snmp_set_valueretrieval(int method)
   Specify the method how the SNMP values will be returned */
PHP_FUNCTION(snmp_set_valueretrieval)
{
	long method;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &method) == FAILURE) {
		RETURN_FALSE;
	}

	if (method >= 0 && method <= (SNMP_VALUE_LIBRARY|SNMP_VALUE_PLAIN|SNMP_VALUE_OBJECT)) {
			SNMP_G(valueretrieval) = method;
			RETURN_TRUE;
	} else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown SNMP value retrieval method '%ld'", method);
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto int snmp_get_valueretrieval()
   Return the method how the SNMP values will be returned */
PHP_FUNCTION(snmp_get_valueretrieval)
{
	if (zend_parse_parameters_none() == FAILURE) {
		RETURN_FALSE;
	}

	RETURN_LONG(SNMP_G(valueretrieval));
}
/* }}} */

/* {{{ proto bool snmp_read_mib(string filename)
   Reads and parses a MIB file into the active MIB tree. */
PHP_FUNCTION(snmp_read_mib)
{
	char *filename;
	int filename_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "p", &filename, &filename_len) == FAILURE) {
		RETURN_FALSE;
	}

	if (!read_mib(filename)) {
		char *error = strerror(errno);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Error while reading MIB file '%s': %s", filename, error);
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto SNMP SNMP::__construct(int version, string hostname, string community|securityName [, long timeout [, long retries]])
	Creates a new SNMP session to specified host. */
PHP_METHOD(snmp, __construct)
{
	php_snmp_object *snmp_object;
	zval *object = getThis();
	char *a1, *a2;
	int a1_len, a2_len;
	long timeout = SNMP_DEFAULT_TIMEOUT;
	long retries = SNMP_DEFAULT_RETRIES;
	long version = SNMP_DEFAULT_VERSION;
	int argc = ZEND_NUM_ARGS();
	zend_error_handling error_handling;

	snmp_object = (php_snmp_object *)zend_object_store_get_object(object TSRMLS_CC);
	zend_replace_error_handling(EH_THROW, NULL, &error_handling TSRMLS_CC);
	
	if (zend_parse_parameters(argc TSRMLS_CC, "lss|ll", &version, &a1, &a1_len, &a2, &a2_len, &timeout, &retries) == FAILURE) {
		zend_restore_error_handling(&error_handling TSRMLS_CC);
		return;
	}

	zend_restore_error_handling(&error_handling TSRMLS_CC);

	switch(version) {
		case SNMP_VERSION_1:
		case SNMP_VERSION_2c:
		case SNMP_VERSION_3:
			break;
		default:
			zend_throw_exception(zend_exception_get_default(TSRMLS_C), "Unknown SNMP protocol version", 0 TSRMLS_CC);
			return;
	}

	/* handle re-open of snmp session */
	if (snmp_object->session) {
		netsnmp_session_free(&(snmp_object->session));
	}
	
	if (netsnmp_session_init(&(snmp_object->session), version, a1, a2, timeout, retries TSRMLS_CC)) {
		return;
	}
	snmp_object->max_oids = 0;
	snmp_object->valueretrieval = SNMP_G(valueretrieval);
	snmp_object->enum_print = netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PRINT_NUMERIC_ENUM);
	snmp_object->oid_output_format = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT);
	snmp_object->quick_print = netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_QUICK_PRINT);
	snmp_object->oid_increasing_check = TRUE;
	snmp_object->exceptions_enabled = 0;
}
/* }}} */

/* {{{ proto bool SNMP::close() 
	Close SNMP session */
PHP_METHOD(snmp, close)
{
	php_snmp_object *snmp_object;
	zval *object = getThis();

	snmp_object = (php_snmp_object *)zend_object_store_get_object(object TSRMLS_CC);

	if (zend_parse_parameters_none() == FAILURE) {
		RETURN_FALSE;
	}

	netsnmp_session_free(&(snmp_object->session));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto mixed SNMP::get(mixed object_id [, bool preserve_keys])
   Fetch a SNMP object returing scalar for single OID and array of oid->value pairs for multi OID request */
PHP_METHOD(snmp, get)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, SNMP_CMD_GET, (-1));
}
/* }}} */

/* {{{ proto mixed SNMP::getnext(mixed object_id)
   Fetch a SNMP object returing scalar for single OID and array of oid->value pairs for multi OID request */
PHP_METHOD(snmp, getnext)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, SNMP_CMD_GETNEXT, (-1));
}
/* }}} */

/* {{{ proto mixed SNMP::walk(mixed object_id [, bool $suffix_as_key = FALSE [, int $max_repetitions [, int $non_repeaters]])
   Return all objects including their respective object id withing the specified one as array of oid->value pairs */
PHP_METHOD(snmp, walk)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, SNMP_CMD_WALK, (-1));
}
/* }}} */

/* {{{ proto bool SNMP::set(mixed object_id, mixed type, mixed value) 
   Set the value of a SNMP object */
PHP_METHOD(snmp, set)
{
	php_snmp(INTERNAL_FUNCTION_PARAM_PASSTHRU, SNMP_CMD_SET, (-1));
}

/* {{{ proto bool SNMP::setSecurity(string sec_level, [ string auth_protocol, string auth_passphrase [, string priv_protocol, string priv_passphrase [, string contextName [, string contextEngineID]]]])
	Set SNMPv3 security-related session parameters */
PHP_METHOD(snmp, setSecurity)
{
	php_snmp_object *snmp_object;
	zval *object = getThis();
	char *a1 = "", *a2 = "", *a3 = "", *a4 = "", *a5 = "", *a6 = "", *a7 = "";
	int a1_len = 0, a2_len = 0, a3_len = 0, a4_len = 0, a5_len = 0, a6_len = 0, a7_len = 0;
	int argc = ZEND_NUM_ARGS();

	snmp_object = (php_snmp_object *)zend_object_store_get_object(object TSRMLS_CC);
	
	if (zend_parse_parameters(argc TSRMLS_CC, "s|ssssss", &a1, &a1_len, &a2, &a2_len, &a3, &a3_len,
		&a4, &a4_len, &a5, &a5_len, &a6, &a6_len, &a7, &a7_len) == FAILURE) {
		RETURN_FALSE;
	}

	if (netsnmp_session_set_security(snmp_object->session, a1, a2, a3, a4, a5, a6, a7 TSRMLS_CC)) {
		/* Warning message sent already, just bail out */
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto long SNMP::getErrno() 
	Get last error code number */
PHP_METHOD(snmp, getErrno)
{
	php_snmp_object *snmp_object;
	zval *object = getThis();

	snmp_object = (php_snmp_object *)zend_object_store_get_object(object TSRMLS_CC);

	RETVAL_LONG(snmp_object->snmp_errno);
	return;
}
/* }}} */

/* {{{ proto long SNMP::getError() 
	Get last error message */
PHP_METHOD(snmp, getError)
{
	php_snmp_object *snmp_object;
	zval *object = getThis();

	snmp_object = (php_snmp_object *)zend_object_store_get_object(object TSRMLS_CC);

	RETVAL_STRING(snmp_object->snmp_errstr, 1);
	return;
}
/* }}} */

/* {{{ */
void php_snmp_add_property(HashTable *h, const char *name, size_t name_length, php_snmp_read_t read_func, php_snmp_write_t write_func TSRMLS_DC)
{
	php_snmp_prop_handler p;

	p.name = (char*) name;
	p.name_length = name_length;
	p.read_func = (read_func) ? read_func : NULL;
	p.write_func = (write_func) ? write_func : NULL;
	zend_hash_add(h, (char *)name, name_length + 1, &p, sizeof(php_snmp_prop_handler), NULL);
}
/* }}} */

/* {{{ php_snmp_read_property(zval *object, zval *member, int type[, const zend_literal *key])
   Generic object property reader */
zval *php_snmp_read_property(zval *object, zval *member, int type, const zend_literal *key TSRMLS_DC)
{
	zval tmp_member;
	zval *retval;
	php_snmp_object *obj;
	php_snmp_prop_handler *hnd;
	int ret;

	ret = FAILURE;
	obj = (php_snmp_object *)zend_objects_get_address(object TSRMLS_CC);

	if (Z_TYPE_P(member) != IS_STRING) {
		tmp_member = *member;
		zval_copy_ctor(&tmp_member);
		convert_to_string(&tmp_member);
		member = &tmp_member;
	}

	ret = zend_hash_find(&php_snmp_properties, Z_STRVAL_P(member), Z_STRLEN_P(member)+1, (void **) &hnd);

	if (ret == SUCCESS && hnd->read_func) {
		ret = hnd->read_func(obj, &retval TSRMLS_CC);
		if (ret == SUCCESS) {
			/* ensure we're creating a temporary variable */
			Z_SET_REFCOUNT_P(retval, 0);
		} else {
			retval = EG(uninitialized_zval_ptr);
		}
	} else {
		zend_object_handlers * std_hnd = zend_get_std_object_handlers();
		retval = std_hnd->read_property(object, member, type, key TSRMLS_CC);
	}

	if (member == &tmp_member) {
		zval_dtor(member);
	}
	return(retval);
}
/* }}} */

/* {{{ php_snmp_write_property(zval *object, zval *member, zval *value[, const zend_literal *key])
   Generic object property writer */
void php_snmp_write_property(zval *object, zval *member, zval *value, const zend_literal *key TSRMLS_DC)
{
	zval tmp_member;
	php_snmp_object *obj;
	php_snmp_prop_handler *hnd;
	int ret;

	if (Z_TYPE_P(member) != IS_STRING) {
		tmp_member = *member;
		zval_copy_ctor(&tmp_member);
		convert_to_string(&tmp_member);
		member = &tmp_member;
	}

	ret = FAILURE;
	obj = (php_snmp_object *)zend_objects_get_address(object TSRMLS_CC);

	ret = zend_hash_find(&php_snmp_properties, Z_STRVAL_P(member), Z_STRLEN_P(member) + 1, (void **) &hnd);

	if (ret == SUCCESS && hnd->write_func) {
		hnd->write_func(obj, value TSRMLS_CC);
		if (! PZVAL_IS_REF(value) && Z_REFCOUNT_P(value) == 0) {
			Z_ADDREF_P(value);
			zval_ptr_dtor(&value);
		}
	} else {
		zend_object_handlers * std_hnd = zend_get_std_object_handlers();
		std_hnd->write_property(object, member, value, key TSRMLS_CC);
	}

	if (member == &tmp_member) {
		zval_dtor(member);
	}
}
/* }}} */

/* {{{ php_snmp_has_property(zval *object, zval *member, int has_set_exists[, const zend_literal *key])
   Generic object property checker */
static int php_snmp_has_property(zval *object, zval *member, int has_set_exists, const zend_literal *key TSRMLS_DC)
{
	php_snmp_prop_handler *hnd;
	int ret = 0;

	if (zend_hash_find(&php_snmp_properties, Z_STRVAL_P(member), Z_STRLEN_P(member) + 1, (void **)&hnd) == SUCCESS) {
		switch (has_set_exists) {
			case 2:
				ret = 1;
				break;
			case 0: {
				zval *value = php_snmp_read_property(object, member, BP_VAR_IS, key TSRMLS_CC);
				if (value != EG(uninitialized_zval_ptr)) {
					ret = Z_TYPE_P(value) != IS_NULL? 1:0;
					/* refcount is 0 */
					Z_ADDREF_P(value);
					zval_ptr_dtor(&value);
				}
				break;
			}
			default: {
				zval *value = php_snmp_read_property(object, member, BP_VAR_IS, key TSRMLS_CC);
				if (value != EG(uninitialized_zval_ptr)) {
					convert_to_boolean(value);
					ret = Z_BVAL_P(value)? 1:0;
					/* refcount is 0 */
					Z_ADDREF_P(value);
					zval_ptr_dtor(&value);
				}
				break;
			}
		}
	} else {
		zend_object_handlers * std_hnd = zend_get_std_object_handlers();
		ret = std_hnd->has_property(object, member, has_set_exists, key TSRMLS_CC);
	}
	return ret;
}
/* }}} */

/* {{{ php_snmp_get_properties(zval *object)
   Returns all object properties. Injects SNMP properties into object on first call */
static HashTable *php_snmp_get_properties(zval *object TSRMLS_DC)
{
	php_snmp_object *obj;
	php_snmp_prop_handler *hnd;
	HashTable *props;
	zval *val;
	char *key;
	uint key_len;
	HashPosition pos;
	ulong num_key;

	obj = (php_snmp_object *)zend_objects_get_address(object TSRMLS_CC);
	props = zend_std_get_properties(object TSRMLS_CC);

	zend_hash_internal_pointer_reset_ex(&php_snmp_properties, &pos);

	while (zend_hash_get_current_data_ex(&php_snmp_properties, (void**)&hnd, &pos) == SUCCESS) {
		zend_hash_get_current_key_ex(&php_snmp_properties, &key, &key_len, &num_key, 0, &pos);
		if (!hnd->read_func || hnd->read_func(obj, &val TSRMLS_CC) != SUCCESS) {
			val = EG(uninitialized_zval_ptr);
			Z_ADDREF_P(val);
		}
		zend_hash_update(props, key, key_len, (void *)&val, sizeof(zval *), NULL);
		zend_hash_move_forward_ex(&php_snmp_properties, &pos);
	}
	return obj->zo.properties;
}
/* }}} */

/* {{{ */
static int php_snmp_read_info(php_snmp_object *snmp_object, zval **retval TSRMLS_DC)
{
	zval *val;

	MAKE_STD_ZVAL(*retval);
	array_init(*retval);

	if (snmp_object->session == NULL) {
		return SUCCESS;
	}
		
	MAKE_STD_ZVAL(val);
	ZVAL_STRINGL(val, snmp_object->session->peername, strlen(snmp_object->session->peername), 1);
	add_assoc_zval(*retval, "hostname", val);
	
	MAKE_STD_ZVAL(val);
	ZVAL_LONG(val, snmp_object->session->remote_port);
	add_assoc_zval(*retval, "port", val);
	
	MAKE_STD_ZVAL(val);
	ZVAL_LONG(val, snmp_object->session->timeout);
	add_assoc_zval(*retval, "timeout", val);
	
	MAKE_STD_ZVAL(val);
	ZVAL_LONG(val, snmp_object->session->retries);
	add_assoc_zval(*retval, "retries", val);
	
	return SUCCESS;
}
/* }}} */

/* {{{ */
static int php_snmp_read_max_oids(php_snmp_object *snmp_object, zval **retval TSRMLS_DC)
{
	MAKE_STD_ZVAL(*retval);
	if (snmp_object->max_oids > 0) {
		ZVAL_LONG(*retval, snmp_object->max_oids);
	} else {
		ZVAL_NULL(*retval);
	}
	return SUCCESS;
}
/* }}} */

#define PHP_SNMP_BOOL_PROPERTY_READER_FUNCTION(name) \
	static int php_snmp_read_##name(php_snmp_object *snmp_object, zval **retval TSRMLS_DC) \
	{ \
		MAKE_STD_ZVAL(*retval); \
		ZVAL_BOOL(*retval, snmp_object->name); \
		return SUCCESS; \
	}

PHP_SNMP_BOOL_PROPERTY_READER_FUNCTION(oid_increasing_check)
PHP_SNMP_BOOL_PROPERTY_READER_FUNCTION(quick_print)
PHP_SNMP_BOOL_PROPERTY_READER_FUNCTION(enum_print)

#define PHP_SNMP_LONG_PROPERTY_READER_FUNCTION(name) \
	static int php_snmp_read_##name(php_snmp_object *snmp_object, zval **retval TSRMLS_DC) \
	{ \
		MAKE_STD_ZVAL(*retval); \
		ZVAL_LONG(*retval, snmp_object->name); \
		return SUCCESS; \
	}

PHP_SNMP_LONG_PROPERTY_READER_FUNCTION(valueretrieval)
PHP_SNMP_LONG_PROPERTY_READER_FUNCTION(oid_output_format)
PHP_SNMP_LONG_PROPERTY_READER_FUNCTION(exceptions_enabled)

/* {{{ */
static int php_snmp_write_info(php_snmp_object *snmp_object, zval *newval TSRMLS_DC)
{
	php_error_docref(NULL TSRMLS_CC, E_WARNING, "info property is read-only");
	return FAILURE;
}
/* }}} */

/* {{{ */
static int php_snmp_write_max_oids(php_snmp_object *snmp_object, zval *newval TSRMLS_DC)
{
	zval ztmp;
	int ret = SUCCESS;

	if (Z_TYPE_P(newval) == IS_NULL) {
		snmp_object->max_oids = 0;
		return ret;
	}

	if (Z_TYPE_P(newval) != IS_LONG) {
		ztmp = *newval;
		zval_copy_ctor(&ztmp);
		convert_to_long(&ztmp);
		newval = &ztmp;
	}

	if (Z_LVAL_P(newval) > 0) {
		snmp_object->max_oids = Z_LVAL_P(newval);
	} else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "max_oids should be positive integer or NULL, got %ld", Z_LVAL_P(newval));
	}
	
	if (newval == &ztmp) {
		zval_dtor(newval);
	}

	return ret;
}
/* }}} */

/* {{{ */
static int php_snmp_write_valueretrieval(php_snmp_object *snmp_object, zval *newval TSRMLS_DC)
{
	zval ztmp;
	int ret = SUCCESS;

	if (Z_TYPE_P(newval) != IS_LONG) {
		ztmp = *newval;
		zval_copy_ctor(&ztmp);
		convert_to_long(&ztmp);
		newval = &ztmp;
	}

	if (Z_LVAL_P(newval) >= 0 && Z_LVAL_P(newval) <= (SNMP_VALUE_LIBRARY|SNMP_VALUE_PLAIN|SNMP_VALUE_OBJECT)) {
		snmp_object->valueretrieval = Z_LVAL_P(newval);
	} else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown SNMP value retrieval method '%ld'", Z_LVAL_P(newval));
		ret = FAILURE;
	}
	
	if (newval == &ztmp) {
		zval_dtor(newval);
	}

	return ret;
}
/* }}} */

#define PHP_SNMP_BOOL_PROPERTY_WRITER_FUNCTION(name) \
static int php_snmp_write_##name(php_snmp_object *snmp_object, zval *newval TSRMLS_DC) \
{ \
	zval ztmp; \
	if (Z_TYPE_P(newval) != IS_BOOL) { \
		ztmp = *newval; \
		zval_copy_ctor(&ztmp); \
		convert_to_boolean(&ztmp); \
		newval = &ztmp; \
	} \
\
	snmp_object->name = Z_LVAL_P(newval); \
\
	if (newval == &ztmp) { \
		zval_dtor(newval); \
	} \
	return SUCCESS; \
}

PHP_SNMP_BOOL_PROPERTY_WRITER_FUNCTION(quick_print)
PHP_SNMP_BOOL_PROPERTY_WRITER_FUNCTION(enum_print)
PHP_SNMP_BOOL_PROPERTY_WRITER_FUNCTION(oid_increasing_check)

/* {{{ */
static int php_snmp_write_oid_output_format(php_snmp_object *snmp_object, zval *newval TSRMLS_DC)
{
	zval ztmp;
	int ret = SUCCESS;
	if (Z_TYPE_P(newval) != IS_LONG) {
		ztmp = *newval;
		zval_copy_ctor(&ztmp);
		convert_to_long(&ztmp);
		newval = &ztmp;
	}
	
	switch(Z_LVAL_P(newval)) {
		case NETSNMP_OID_OUTPUT_SUFFIX:
		case NETSNMP_OID_OUTPUT_MODULE:
		case NETSNMP_OID_OUTPUT_FULL:
		case NETSNMP_OID_OUTPUT_NUMERIC:
		case NETSNMP_OID_OUTPUT_UCD:
		case NETSNMP_OID_OUTPUT_NONE:
			snmp_object->oid_output_format = Z_LVAL_P(newval);
			break;
		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown SNMP output print format '%ld'", Z_LVAL_P(newval));
			ret = FAILURE;
			break;
	}

	if (newval == &ztmp) {
		zval_dtor(newval);
	}
	return ret;
}
/* }}} */

/* {{{ */
static int php_snmp_write_exceptions_enabled(php_snmp_object *snmp_object, zval *newval TSRMLS_DC)
{
	zval ztmp;
	int ret = SUCCESS;
	if (Z_TYPE_P(newval) != IS_LONG) {
		ztmp = *newval;
		zval_copy_ctor(&ztmp);
		convert_to_long(&ztmp);
		newval = &ztmp;
	}

	snmp_object->exceptions_enabled = Z_LVAL_P(newval);	

	if (newval == &ztmp) {
		zval_dtor(newval);
	}
	return ret;
}
/* }}} */

/* {{{ php_snmp_class_methods[] */
static zend_function_entry php_snmp_class_methods[] = {
	PHP_ME(snmp,	 __construct,			arginfo_snmp_create,		ZEND_ACC_PUBLIC)
	PHP_ME(snmp,	 close,				arginfo_snmp_void,		ZEND_ACC_PUBLIC)
	PHP_ME(snmp,	 setSecurity,			arginfo_snmp_setSecurity,	ZEND_ACC_PUBLIC)

	PHP_ME(snmp,	 get,				arginfo_snmp_get,		ZEND_ACC_PUBLIC)
	PHP_ME(snmp,	 getnext,			arginfo_snmp_get,		ZEND_ACC_PUBLIC)
	PHP_ME(snmp,	 walk,				arginfo_snmp_walk,		ZEND_ACC_PUBLIC)
	PHP_ME(snmp,	 set,				arginfo_snmp_set,		ZEND_ACC_PUBLIC)
	PHP_ME(snmp,	 getErrno,			arginfo_snmp_void,		ZEND_ACC_PUBLIC)
	PHP_ME(snmp,	 getError,			arginfo_snmp_void,		ZEND_ACC_PUBLIC)

	PHP_FE_END
};

#define PHP_SNMP_PROPERTY_ENTRY_RECORD(name) \
	{ "" #name "",		sizeof("" #name "") - 1,	php_snmp_read_##name,	php_snmp_write_##name }

const php_snmp_prop_handler php_snmp_property_entries[] = {
	PHP_SNMP_PROPERTY_ENTRY_RECORD(info),
	PHP_SNMP_PROPERTY_ENTRY_RECORD(max_oids),
	PHP_SNMP_PROPERTY_ENTRY_RECORD(valueretrieval),
	PHP_SNMP_PROPERTY_ENTRY_RECORD(quick_print),
	PHP_SNMP_PROPERTY_ENTRY_RECORD(enum_print),
	PHP_SNMP_PROPERTY_ENTRY_RECORD(oid_output_format),
	PHP_SNMP_PROPERTY_ENTRY_RECORD(oid_increasing_check),
	PHP_SNMP_PROPERTY_ENTRY_RECORD(exceptions_enabled),
	{ NULL, 0, NULL, NULL}
};
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(snmp)
{
	netsnmp_log_handler *logh;
	zend_class_entry ce, cex;

	le_snmp_session = zend_register_list_destructors_ex(php_snmp_session_destructor, NULL, PHP_SNMP_SESSION_RES_NAME, module_number);

	init_snmp("snmpapp");

#ifdef NETSNMP_DS_LIB_DONT_PERSIST_STATE
	/* Prevent update of the snmpapp.conf file */
	netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_PERSIST_STATE, 1);
#endif

	/* Disable logging, use exit status'es and related variabled to detect errors */
	shutdown_snmp_logging();
	logh = netsnmp_register_loghandler(NETSNMP_LOGHANDLER_NONE, LOG_ERR);
	if (logh) {
		logh->pri_max = LOG_ERR;
	}

	memcpy(&php_snmp_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_snmp_object_handlers.read_property = php_snmp_read_property;
	php_snmp_object_handlers.write_property = php_snmp_write_property;
	php_snmp_object_handlers.has_property = php_snmp_has_property;
	php_snmp_object_handlers.get_properties = php_snmp_get_properties;

	/* Register SNMP Class */
	INIT_CLASS_ENTRY(ce, "SNMP", php_snmp_class_methods);
	ce.create_object = php_snmp_object_new;
	php_snmp_object_handlers.clone_obj = NULL;
	php_snmp_ce = zend_register_internal_class(&ce TSRMLS_CC);

	/* Register SNMP Class properties */
	zend_hash_init(&php_snmp_properties, 0, NULL, NULL, 1);
	PHP_SNMP_ADD_PROPERTIES(&php_snmp_properties, php_snmp_property_entries);

	REGISTER_LONG_CONSTANT("SNMP_OID_OUTPUT_SUFFIX",	NETSNMP_OID_OUTPUT_SUFFIX,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SNMP_OID_OUTPUT_MODULE",	NETSNMP_OID_OUTPUT_MODULE,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SNMP_OID_OUTPUT_FULL",		NETSNMP_OID_OUTPUT_FULL,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SNMP_OID_OUTPUT_NUMERIC",	NETSNMP_OID_OUTPUT_NUMERIC,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SNMP_OID_OUTPUT_UCD",		NETSNMP_OID_OUTPUT_UCD,		CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SNMP_OID_OUTPUT_NONE",		NETSNMP_OID_OUTPUT_NONE,	CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("SNMP_VALUE_LIBRARY",	SNMP_VALUE_LIBRARY,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SNMP_VALUE_PLAIN",	SNMP_VALUE_PLAIN,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SNMP_VALUE_OBJECT",	SNMP_VALUE_OBJECT,	CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("SNMP_BIT_STR",		ASN_BIT_STR,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SNMP_OCTET_STR",	ASN_OCTET_STR,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SNMP_OPAQUE",		ASN_OPAQUE,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SNMP_NULL",		ASN_NULL,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SNMP_OBJECT_ID",	ASN_OBJECT_ID,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SNMP_IPADDRESS",	ASN_IPADDRESS,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SNMP_COUNTER",		ASN_GAUGE,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SNMP_UNSIGNED",		ASN_UNSIGNED,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SNMP_TIMETICKS",	ASN_TIMETICKS,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SNMP_UINTEGER",		ASN_UINTEGER,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SNMP_INTEGER",		ASN_INTEGER,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SNMP_COUNTER64",	ASN_COUNTER64,	CONST_CS | CONST_PERSISTENT);

	REGISTER_SNMP_CLASS_CONST_LONG("VERSION_1",			SNMP_VERSION_1);
	REGISTER_SNMP_CLASS_CONST_LONG("VERSION_2c",			SNMP_VERSION_2c);
	REGISTER_SNMP_CLASS_CONST_LONG("VERSION_2C",			SNMP_VERSION_2c);
	REGISTER_SNMP_CLASS_CONST_LONG("VERSION_3",			SNMP_VERSION_3);

	REGISTER_SNMP_CLASS_CONST_LONG("ERRNO_NOERROR",			PHP_SNMP_ERRNO_NOERROR);
	REGISTER_SNMP_CLASS_CONST_LONG("ERRNO_ANY",			PHP_SNMP_ERRNO_ANY);
	REGISTER_SNMP_CLASS_CONST_LONG("ERRNO_GENERIC",			PHP_SNMP_ERRNO_GENERIC);
	REGISTER_SNMP_CLASS_CONST_LONG("ERRNO_TIMEOUT",			PHP_SNMP_ERRNO_TIMEOUT);
	REGISTER_SNMP_CLASS_CONST_LONG("ERRNO_ERROR_IN_REPLY",		PHP_SNMP_ERRNO_ERROR_IN_REPLY);
	REGISTER_SNMP_CLASS_CONST_LONG("ERRNO_OID_NOT_INCREASING",	PHP_SNMP_ERRNO_OID_NOT_INCREASING);
	REGISTER_SNMP_CLASS_CONST_LONG("ERRNO_OID_PARSING_ERROR",	PHP_SNMP_ERRNO_OID_PARSING_ERROR);
	REGISTER_SNMP_CLASS_CONST_LONG("ERRNO_MULTIPLE_SET_QUERIES",	PHP_SNMP_ERRNO_MULTIPLE_SET_QUERIES);

	/* Register SNMPException class */
	INIT_CLASS_ENTRY(cex, "SNMPException", NULL);
#ifdef HAVE_SPL
	php_snmp_exception_ce = zend_register_internal_class_ex(&cex, spl_ce_RuntimeException, NULL TSRMLS_CC);
#else
	php_snmp_exception_ce = zend_register_internal_class_ex(&cex, zend_exception_get_default(TSRMLS_C), NULL TSRMLS_CC);
#endif

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(snmp)
{
	snmp_shutdown("snmpapp");
	
	zend_hash_destroy(&php_snmp_properties);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(snmp)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "NET-SNMP Support", "enabled");
	php_info_print_table_row(2, "NET-SNMP Version", netsnmp_get_version());
	php_info_print_table_row(2, "PHP SNMP Version", PHP_SNMP_VERSION);
	php_info_print_table_end();
}
/* }}} */

/* {{{ snmp_module_deps[]
 */
static const zend_module_dep snmp_module_deps[] = {
#ifdef HAVE_SPL
	ZEND_MOD_REQUIRED("spl")
#endif
	ZEND_MOD_END
};
/* }}} */

/* {{{ snmp_module_entry
 */
zend_module_entry snmp_module_entry = {
	STANDARD_MODULE_HEADER_EX,
	NULL,
	snmp_module_deps,
	"snmp",
	snmp_functions,
	PHP_MINIT(snmp),
	PHP_MSHUTDOWN(snmp),
	NULL,
	NULL,
	PHP_MINFO(snmp),
	PHP_SNMP_VERSION,
	PHP_MODULE_GLOBALS(snmp),
	PHP_GINIT(snmp),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
