/*
   +----------------------------------------------------------------------+
   | PHP version 4.0                                                      |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997, 1998, 1999, 2000 The PHP Group                   |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.0 of the PHP license,       |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license.html.                                     |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Sam Ruby (rubys@us.ibm.com)                                  |
   +----------------------------------------------------------------------+
 */

/*
 * This module implements Zend OO syntax overloading support for Java
 * components using JNI and reflection.
 */

#include "dl/phpdl.h"

#include "php.h"
#include "zend_compile.h"
#include "php_ini.h"
#include "php_globals.h"

#include <jni.h>

#include <stdio.h>

#define IS_EXCEPTION 86

/***************************************************************************/

#ifndef KAFFE
#ifndef JNI_11
#ifndef JNI_12

#ifdef JNI_VERSION_1_2
#define JNI_12
#else
#define JNI_11
#endif

#endif
#endif
#endif

#ifdef PHP_WIN32
#ifdef JNI_12
#pragma comment(lib,"jvm.lib")
#else
#pragma comment(lib,"javai.lib")
#endif
#else
static void *javadl = 0;
#endif

/***************************************************************************/

static int le_jobject = 0;

static char *javalib   = 0;
static char *classpath = 0;
static char *libpath   = 0;
static char *javahome  = 0;

static int iniUpdated  = 0;

static JavaVM *jvm = 0;
static JNIEnv *jenv = 0;
static jclass php_reflect;

static zend_class_entry java_class_entry;

static PHP_INI_MH(OnIniUpdate) {
  if (new_value) *(char**)mh_arg1 = new_value;
  iniUpdated=1;
  return SUCCESS;
}

PHP_INI_BEGIN()
  PHP_INI_ENTRY1("java.class.path",
    NULL, PHP_INI_ALL, OnIniUpdate, &classpath)
  PHP_INI_ENTRY1("java.home",
    NULL, PHP_INI_ALL, OnIniUpdate, &javahome)
  PHP_INI_ENTRY1("java.library",
    NULL, PHP_INI_ALL, OnIniUpdate, &javalib)
  PHP_INI_ENTRY1("java.library.path",
    NULL, PHP_INI_ALL, OnIniUpdate, &libpath)
PHP_INI_END()

/***************************************************************************/

/*
 * Destroy a Java Virtual Machine.
 */
void jvm_destroy() {
  if (php_reflect) (*jenv)->DeleteGlobalRef(jenv, php_reflect);
  if (jvm) {
    (*jvm)->DetachCurrentThread(jvm);
    (*jvm)->DestroyJavaVM(jvm);
    jvm = 0;
  }
#ifndef PHP_WIN32
  if (javadl) dlclose(javadl);
#endif
  php_reflect = 0;
  jenv = 0;
}

/*
 * Create a Java Virtual Machine.
 *  - class.path, home, and library.path are read out of the INI file
 *  - appropriate (pre 1.1, JDK 1.1, and JDK 1.2) initialization is performed
 *  - net.php.reflect class file is located
 */

#ifdef JNI_12
static void addJVMOption(JavaVMInitArgs *vm_args, char *name, char *value) {
  char *option = (char*) malloc(strlen(name) + strlen(value) + 1);
  strcpy(option, name);
  strcat(option, value);
  vm_args->options[vm_args->nOptions++].optionString = option;
}
#endif

static int jvm_create() {

  int rc;
  jclass local_php_reflect;
  jthrowable error;

#ifdef JNI_11
  JDK1_1InitArgs vm_args;
#else
  JavaVMInitArgs vm_args;
#ifdef JNI_12
  JavaVMOption options[3];
#endif
#endif

  iniUpdated=0;

  if (!classpath) classpath = getenv("CLASSPATH");

#ifndef PHP_WIN32
  if (!libpath)   libpath   = getenv("LD_LIBRARY_PATH");
  if (javalib) {
    javadl = dlopen(javalib, RTLD_GLOBAL | RTLD_LAZY);
    if (!javadl) {
      php_error(E_ERROR, "Unable to create Java Virtual Machine");
      return -1;
    }
  }
#endif

#ifdef JNI_12

  vm_args.version = JNI_VERSION_1_2;
  vm_args.ignoreUnrecognized = JNI_FALSE;
  vm_args.options = options;
  vm_args.nOptions = 0;

  if (classpath) addJVMOption(&vm_args, "-Djava.class.path=",   classpath);
  if (javahome)  addJVMOption(&vm_args, "-Djava.home=",         javahome);
  if (libpath)   addJVMOption(&vm_args, "-Djava.library.path=", libpath);

  rc = JNI_CreateJavaVM(&jvm, (void**)&jenv, &vm_args);

#else

  vm_args.version=0x00010001;
  JNI_GetDefaultJavaVMInitArgs(&vm_args);

  if (!classpath) classpath = "";
  vm_args.classpath = classpath;
#ifdef KAFFE
  vm_args.classhome = javahome;
  vm_args.libraryhome = libpath;
#endif
  rc = JNI_CreateJavaVM(&jvm, &jenv, &vm_args);

#endif

  if (rc) {
    php_error(E_ERROR, "Unable to create Java Virtual Machine");
    return rc;
  }

  local_php_reflect = (*jenv)->FindClass(jenv, "net/php/reflect");
  error = (*jenv)->ExceptionOccurred(jenv);
  if (error) {
    jclass errClass = (*jenv)->GetObjectClass(jenv, error);
    jmethodID toString = (*jenv)->GetMethodID(jenv, errClass, "toString",
      "()Ljava/lang/String;");
    jobject errString = (*jenv)->CallObjectMethod(jenv, error, toString);
    const char *errAsUTF = (*jenv)->GetStringUTFChars(jenv, errString, 0);
    php_error(E_ERROR, "%s", errAsUTF);
    (*jenv)->ReleaseStringUTFChars(jenv, error, errAsUTF);
    (*jenv)->ExceptionClear(jenv);
    jvm_destroy();
    return -1;
  }

  php_reflect = (*jenv)->NewGlobalRef(jenv, local_php_reflect);
  return rc;
}

/***************************************************************************/

static jobjectArray _java_makeArray(int argc, pval** argv) {
  jclass objectClass = (*jenv)->FindClass(jenv, "java/lang/Object");
  jobjectArray result = (*jenv)->NewObjectArray(jenv, argc, objectClass, 0);
  jobject arg;
  jmethodID makeArg;
  int i;
  pval **handle;
  int type;

  for (i=0; i<argc; i++) {
    switch (argv[i]->type) {
      case IS_STRING:
        arg=(*jenv)->NewStringUTF(jenv,argv[i]->value.str.val);
        break;

      case IS_OBJECT:
        zend_hash_index_find(argv[i]->value.obj.properties, 0, (void*)&handle);
        arg = zend_list_find((*handle)->value.lval, &type);
        break;

      case IS_BOOL:
        makeArg = (*jenv)->GetStaticMethodID(jenv, php_reflect, "MakeArg",
          "(Z)Ljava/lang/Object;");
        arg = (*jenv)->CallStaticObjectMethod(jenv, php_reflect, makeArg,
          (jboolean)(argv[i]->value.lval));
        break;

      case IS_LONG:
        makeArg = (*jenv)->GetStaticMethodID(jenv, php_reflect, "MakeArg",
          "(J)Ljava/lang/Object;");
        arg = (*jenv)->CallStaticObjectMethod(jenv, php_reflect, makeArg,
          (jlong)(argv[i]->value.lval));
        break;

      case IS_DOUBLE:
        makeArg = (*jenv)->GetStaticMethodID(jenv, php_reflect, "MakeArg",
          "(D)Ljava/lang/Object;");
        arg = (*jenv)->CallStaticObjectMethod(jenv, php_reflect, makeArg,
          (jdouble)(argv[i]->value.dval));
        break;

      default:
        arg=0;
    }
    (*jenv)->SetObjectArrayElement(jenv, result, i, arg);
    if (argv[i]->type != IS_OBJECT)
    (*jenv)->DeleteLocalRef(jenv, arg);
  }
  return result;
}

static int checkError(pval *value) {
  if (value->type == IS_EXCEPTION) {
    php_error(E_WARNING, "%s", value->value.str.val);
    efree(value->value.str.val);
    var_reset(value);
    return 1;
  };
  return 0;
}


/***************************************************************************/

/*
 * Invoke a method on an object.  If method name is "java", create a new
 * object instead.
 */
void java_call_function_handler
  (INTERNAL_FUNCTION_PARAMETERS, zend_property_reference *property_reference)
{
  pval *object = property_reference->object;
  zend_overloaded_element *function_name = (zend_overloaded_element *)
    property_reference->elements_list.tail->data;

  int arg_count = ARG_COUNT(ht);
  jlong result = 0;

  pval **arguments = (pval **) emalloc(sizeof(pval *)*arg_count);
  getParametersArray(ht, arg_count, arguments);

  if (iniUpdated && jenv) jvm_destroy();
  if (!jenv) jvm_create();
  if (!jenv) return;

  if (!strcmp("java",function_name->element.value.str.val)) {

    /* construct a Java object:
       First argument is the class name.  Any additional arguments will
       be treated as constructor parameters. */

    jmethodID co = (*jenv)->GetStaticMethodID(jenv, php_reflect, "CreateObject",
      "(Ljava/lang/String;[Ljava/lang/Object;J)V");
    jstring className=(*jenv)->NewStringUTF(jenv, arguments[0]->value.str.val);
    (pval*)(long)result = object;

    (*jenv)->CallStaticVoidMethod(jenv, php_reflect, co,
      className, _java_makeArray(arg_count-1, arguments+1), result);

    (*jenv)->DeleteLocalRef(jenv, className);

  } else {

    pval **handle;
    int type;
    jobject obj;
    jstring method;

    /* invoke a method on the given object */

    jmethodID invoke = (*jenv)->GetStaticMethodID(jenv, php_reflect, "Invoke",
      "(Ljava/lang/Object;Ljava/lang/String;[Ljava/lang/Object;J)V");
    zend_hash_index_find(object->value.obj.properties, 0, (void**) &handle);
    obj = zend_list_find((*handle)->value.lval, &type);
    method = (*jenv)->NewStringUTF(jenv, function_name->element.value.str.val);
    (pval*)(long)result = return_value;

    (*jenv)->CallStaticVoidMethod(jenv, php_reflect, invoke,
      obj, method, _java_makeArray(arg_count, arguments), result);

    (*jenv)->DeleteLocalRef(jenv, method);

  }

  efree(arguments);
  pval_destructor(&function_name->element);

  checkError((pval*)(long)result);
}

/***************************************************************************/

static pval _java_getset_property
  (zend_property_reference *property_reference, jobjectArray value)
{
  pval presult;
  jlong result = 0;
  pval **pobject;
  jobject obj;
  int type;

  /* get the property name */
  zend_llist_element *element = property_reference->elements_list.head;
  zend_overloaded_element *property=(zend_overloaded_element *)element->data;
  jstring propName =
    (*jenv)->NewStringUTF(jenv, property->element.value.str.val);

  /* get the object */
  zend_hash_index_find(property_reference->object->value.obj.properties,
    0, (void **) &pobject);
  obj = zend_list_find((*pobject)->value.lval,&type);
  (pval*)(long)result = &presult;
  var_uninit(&presult);

  if (!obj || (type!=le_jobject)) {
    php_error(E_ERROR,
      "Attempt to access a Java property on a non-Java object");
  } else {
    /* invoke the method */
    jmethodID gsp = (*jenv)->GetStaticMethodID(jenv, php_reflect, "GetSetProp",
      "(Ljava/lang/Object;Ljava/lang/String;[Ljava/lang/Object;J)V");
    (*jenv)->CallStaticVoidMethod
       (jenv, php_reflect, gsp, obj, propName, value, result);
  }

  (*jenv)->DeleteLocalRef(jenv, propName);
  pval_destructor(&property->element);
  return presult;
}

pval java_get_property_handler
  (zend_property_reference *property_reference)
{
  pval presult = _java_getset_property(property_reference, 0);
  checkError(&presult);
  return presult;
}


int java_set_property_handler
  (zend_property_reference *property_reference, pval *value)
{
  pval presult = _java_getset_property
    (property_reference, _java_makeArray(1, &value));
  return checkError(&presult) ? FAILURE : SUCCESS;
}

/***************************************************************************/

static void _php_java_destructor(void *jobject) {
  if (jenv) (*jenv)->DeleteGlobalRef(jenv, jobject);
}

PHP_MINIT_FUNCTION(java) {
  INIT_OVERLOADED_CLASS_ENTRY(java_class_entry, "java", NULL,
    java_call_function_handler,
    java_get_property_handler,
    java_set_property_handler);

  register_internal_class(&java_class_entry);

  le_jobject = register_list_destructors(_php_java_destructor,NULL);

  REGISTER_INI_ENTRIES();
  return SUCCESS;
}


PHP_MSHUTDOWN_FUNCTION(java) {
  UNREGISTER_INI_ENTRIES();
  if (jvm) jvm_destroy();
  return SUCCESS;
}

function_entry java_functions[] = {
  {NULL, NULL, NULL}
};


static PHP_MINFO_FUNCTION(java) {
  DISPLAY_INI_ENTRIES();
}

zend_module_entry java_module_entry = {
  "java",
  java_functions,
  PHP_MINIT(java),
  PHP_MSHUTDOWN(java),
  NULL,
  NULL,
  PHP_MINFO(java),
  STANDARD_MODULE_PROPERTIES
};

DLEXPORT zend_module_entry *get_module(void) { return &java_module_entry; }

/***************************************************************************/

JNIEXPORT void JNICALL Java_net_php_reflect_setResultFromString
  (JNIEnv *jenv, jclass self, jlong result, jstring value)
{
  const char *valueAsUTF = (*jenv)->GetStringUTFChars(jenv, value, 0);
  pval *presult = (pval*)(long)result;
  presult->type=IS_STRING;
  presult->value.str.len=strlen(valueAsUTF);
  presult->value.str.val=emalloc(presult->value.str.len+1);
  strcpy(presult->value.str.val, valueAsUTF);
  (*jenv)->ReleaseStringUTFChars(jenv, value, valueAsUTF);
}

JNIEXPORT void JNICALL Java_net_php_reflect_setResultFromLong
  (JNIEnv *jenv, jclass self, jlong result, jlong value)
{
  pval *presult = (pval*)(long)result;
  presult->type=IS_LONG;
  presult->value.lval=(long)value;
}

JNIEXPORT void JNICALL Java_net_php_reflect_setResultFromDouble
  (JNIEnv *jenv, jclass self, jlong result, jdouble value)
{
  pval *presult = (pval*)(long)result;
  presult->type=IS_DOUBLE;
  presult->value.dval=value;
}

JNIEXPORT void JNICALL Java_net_php_reflect_setResultFromBoolean
  (JNIEnv *jenv, jclass self, jlong result, jboolean value)
{
  pval *presult = (pval*)(long)result;
  presult->type=IS_BOOL;
  presult->value.lval=value;
}

JNIEXPORT void JNICALL Java_net_php_reflect_setResultFromObject
  (JNIEnv *jenv, jclass self, jlong result, jobject value)
{
  /* wrapper the java object in a pval object */
  pval *presult = (pval*)(long)result;
  pval *handle;

  if (presult->type != IS_OBJECT) {
    presult->type=IS_OBJECT;
    presult->value.obj.ce=&java_class_entry;
    presult->value.obj.properties = (HashTable *) emalloc(sizeof(HashTable));
    presult->is_ref=1;
    presult->refcount=1;
    zend_hash_init(presult->value.obj.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
  };

  ALLOC_ZVAL(handle);
  handle->type = IS_LONG;
  handle->value.lval =
    zend_list_insert((*jenv)->NewGlobalRef(jenv,value), le_jobject);
  pval_copy_constructor(handle);
  INIT_PZVAL(handle);
  zend_hash_index_update(presult->value.obj.properties, 0,
    &handle, sizeof(pval *), NULL);
}

JNIEXPORT void JNICALL Java_net_php_reflect_setException
  (JNIEnv *jenv, jclass self, jlong result, jstring value)
{
  pval *presult = (pval*)(long)result;
  Java_net_php_reflect_setResultFromString(jenv, self, result, value);
  presult->type=IS_EXCEPTION;
}

JNIEXPORT void JNICALL Java_net_php_reflect_setEnv
  (JNIEnv *newJenv, jclass self)
{
  iniUpdated=0;
  jenv=newJenv;
  if (!self) self = (*jenv)->FindClass(jenv, "net/php/reflect");
  php_reflect = (*jenv)->NewGlobalRef(jenv, self);
}
