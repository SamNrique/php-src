--TEST--
Enum: not explicit defined values shoud be an auto-incremented value
--SKIPIF--
<?php if (version_compare(zend_version(), '2.0.0-dev', '<')) die('skip ZendEngine 2 needed'); ?>
--FILE--
<?php

ini_set('display_error', 1);
error_reporting(E_NOTICE|E_ALL);

enum myEnum {
  foo = 3, 
  bar
}

echo myEnum::bar;

?>
--EXPECTF--

4

