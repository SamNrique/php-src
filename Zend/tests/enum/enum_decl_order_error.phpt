--TEST--
Enum: defining a value less or equal than previously defined should make a compialtion error
--SKIPIF--
<?php if (version_compare(zend_version(), '2.0.0-dev', '<')) die('skip ZendEngine 2 needed'); ?>
--FILE--
<?php

ini_set('display_error', 1);
error_reporting(E_NOTICE|E_ALL);

enum myEnum {
  foo = 3, 
  bar = 1
}

echo "Done"; //Should not happened

?>
--EXPECTF--

Fatal error: Cannot define enum value myEnum::bar less than 4 in %s on line %d

