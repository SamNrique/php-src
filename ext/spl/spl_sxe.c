/*
   +----------------------------------------------------------------------+
   | PHP Version 5                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2003 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license,       |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_0.txt.                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Marcus Boerger <helly@php.net>                              |
   +----------------------------------------------------------------------+
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "zend_interfaces.h"

#include "php_spl.h"
#include "spl_functions.h"
#include "spl_engine.h"
#include "spl_iterators.h"
#include "spl_sxe.h"

zend_class_entry *spl_ce_SimpleXMLIterator = NULL;

#if HAVE_LIBXML && HAVE_SIMPLEXML

#include "ext/simplexml/php_simplexml_exports.h"

SPL_METHOD(SimpleXMLIterator, rewind) /* {{{ */
{
	php_sxe_object *sxe = php_sxe_fetch_object(getThis() TSRMLS_CC);

	php_sxe_reset_iterator(sxe TSRMLS_CC);
}
/* }}} */

SPL_METHOD(SimpleXMLIterator, hasMore) /* {{{ */
{
	php_sxe_object *sxe = php_sxe_fetch_object(getThis() TSRMLS_CC);

	RETURN_BOOL(sxe->iter.data);
}
/* }}} */

SPL_METHOD(SimpleXMLIterator, current) /* {{{ */
{
	php_sxe_object *sxe = php_sxe_fetch_object(getThis() TSRMLS_CC);

	if (!sxe->iter.data) {
		return; /* return NULL */
	}

	RETURN_ZVAL(sxe->iter.data, 1, 0);
}
/* }}} */

SPL_METHOD(SimpleXMLIterator, key) /* {{{ */
{
	xmlNodePtr curnode;
	php_sxe_object *intern;
	php_sxe_object *sxe = php_sxe_fetch_object(getThis() TSRMLS_CC);

	if (!sxe->iter.data) {
		RETURN_FALSE;
	}

	intern = (php_sxe_object *)zend_object_store_get_object(sxe->iter.data TSRMLS_CC);
	if (intern != NULL && intern->node != NULL) {
		curnode = (xmlNodePtr)((php_libxml_node_ptr *)intern->node)->node;
		RETURN_STRINGL((char*)curnode->name, xmlStrlen(curnode->name), 1);
	}
    
    RETURN_FALSE;
}
/* }}} */

SPL_METHOD(SimpleXMLIterator, next) /* {{{ */
{
	php_sxe_object *sxe = php_sxe_fetch_object(getThis() TSRMLS_CC);

	php_sxe_move_forward_iterator(sxe TSRMLS_CC);
}
/* }}} */

/* {{{ hasChildren()
 */ 
SPL_METHOD(SimpleXMLIterator, hasChildren)
{
	php_sxe_object *sxe = php_sxe_fetch_object(getThis() TSRMLS_CC);
	php_sxe_object *child;
	xmlNodePtr      node;

	if (!sxe->iter.data) {
		RETURN_FALSE;
	}
	child = php_sxe_fetch_object(sxe->iter.data TSRMLS_CC);

	GET_NODE(child, node);
	if (node) {
		node = node->children;
	}
	while (node && node->type != XML_ELEMENT_NODE) {
		node = node->next;
	}
	RETURN_BOOL(node ? 1 : 0);
}
/* }}} */

/* {{{ getChildren()
 */ 
SPL_METHOD(SimpleXMLIterator, getChildren)
{
	php_sxe_object *sxe = php_sxe_fetch_object(getThis() TSRMLS_CC);

	if (!sxe->iter.data) {
		return; /* return NULL */
	}
	return_value->type = IS_OBJECT;
	return_value->value.obj = zend_objects_store_clone_obj(sxe->iter.data TSRMLS_CC);
}

static zend_function_entry spl_funcs_SimpleXMLIterator[] = {
	SPL_ME(SimpleXMLIterator, rewind,                 NULL, ZEND_ACC_PUBLIC)
	SPL_ME(SimpleXMLIterator, hasMore,                NULL, ZEND_ACC_PUBLIC)
	SPL_ME(SimpleXMLIterator, current,                NULL, ZEND_ACC_PUBLIC)
	SPL_ME(SimpleXMLIterator, key,                    NULL, ZEND_ACC_PUBLIC)
	SPL_ME(SimpleXMLIterator, next,                   NULL, ZEND_ACC_PUBLIC)
	SPL_ME(SimpleXMLIterator, hasChildren,            NULL, ZEND_ACC_PUBLIC)
	SPL_ME(SimpleXMLIterator, getChildren,            NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */

#define SimpleXML_Element sxe_get_element_class_entry()

SPL_API PHP_MINIT_FUNCTION(spl_sxe) /* {{{ */
{
	zend_class_entry *spl_ce_SimpleXML_Element = sxe_get_element_class_entry();

	if (!sxe_get_element_class_entry()) {
		return SUCCESS; /* SimpleXML must be initialized before */
	}

	REGISTER_SPL_SUB_CLASS_EX(SimpleXMLIterator, SimpleXML_Element, sxe_object_new, spl_funcs_SimpleXMLIterator);
	REGISTER_SPL_IMPLEMENTS(SimpleXMLIterator, RecursiveIterator);

	return SUCCESS;
}
/* }}} */

#else /* HAVE_LIBXML && HAVE_SIMPLEXML */

SPL_API PHP_MINIT_FUNCTION(spl_sxe) /* {{{ */
{
	return SUCCESS;
}

#endif /* HAVE_LIBXML && HAVE_SIMPLEXML */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: fdm=marker
 * vim: noet sw=4 ts=4
 */
