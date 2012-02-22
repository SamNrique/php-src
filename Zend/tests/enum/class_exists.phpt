--TEST--
Enum: an enum should'nt be counted as class by class_exists
--SKIPIF--
<?php if (version_compare(zend_version(), '2.0.0-dev', '<')) die('skip ZendEngine 2 needed'); ?>
--FILE--
<?php

enum foo {
  bar = 6
}

class myClass {
}

interface myInterface {
}

var_dump(class_exists("myClass"));
var_dump(class_exists("myInterface"));
var_dump(class_exists("foo"));

?>
--EXPECTF--

bool(true)
bool(false)
bool(false)
