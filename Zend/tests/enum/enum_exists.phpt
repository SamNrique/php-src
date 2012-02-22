--TEST--
Enum: an enum should be counted by enum_exists
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

var_dump(enum_exists("myClass"));
var_dump(enum_exists("myInterface"));
var_dump(enum_exists("foo"));

?>
--EXPECTF--

bool(false)
bool(false)
bool(true)
