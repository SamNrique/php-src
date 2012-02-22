--TEST--
Enum: an enum should be counted by get_declared_enums
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

$interfaceList = get_declared_enums();
var_dump(in_array("myClass", $interfaceList));
var_dump(in_array("myInterface", $interfaceList));
var_dump(in_array("foo", $interfaceList));

?>
--EXPECTF--

bool(false)
bool(false)
bool(true)
