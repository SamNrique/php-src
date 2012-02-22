--TEST--
Enum: Enum::values should return all values defined in this enum declaration
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

var_dump(myEnum::values());

?>
--EXPECTF--
array(2) {
  [0]=>
  int(3)
  [1]=>
  int(4)
}

