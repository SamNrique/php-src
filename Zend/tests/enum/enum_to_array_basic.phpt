--TEST--
Enum: Enum::to_array should return an assiociated arry with all names and values defined in this enum declaration
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

var_dump(myEnum::to_array());

?>
--EXPECTF--
array(2) {
  ["foo"]=>
  int(3)
  ["bar"]=>
  int(4)
}

