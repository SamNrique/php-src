--TEST--
Enum: an enum should'nt be counted as class by get_declared_interfaces
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

$interfaceList = get_declared_interfaces();
var_dump(in_array("myClass", $interfaceList));
var_dump(in_array("myInterface", $interfaceList));
var_dump(in_array("foo", $interfaceList));

?>
--EXPECTF--

bool(false)
bool(true)
bool(false)
