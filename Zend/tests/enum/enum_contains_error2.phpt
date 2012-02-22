--TEST--
Enum: Enum::contains shouldn't work when arg value is not a long castable value
--SKIPIF--
<?php if (version_compare(zend_version(), '2.0.0-dev', '<')) die('skip ZendEngine 2 needed'); ?>
--FILE--
<?php

ini_set('display_error', 1);
error_reporting(E_NOTICE|E_ALL);

enum myEnum {
  foo = 3, 
  bar = 4 
}

var_dump(myEnum::contains("hello world"));

?>
--EXPECTF--

Warning: myEnum::contains() expects parameter 1 to be long, string given in %s on line %d
bool(false)
