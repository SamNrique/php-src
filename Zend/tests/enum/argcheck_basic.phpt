--TEST--
Enum: an enum arg should work when enum const is filled
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

function myFunction(myEnum $arg1) {
  echo $arg1;
}

myFunction(myEnum::foo);

?>
--EXPECTF--

3

