--TEST--
Enum: an enum should'nt be counted as class by get_declared_classes
--SKIPIF--
<?php if (version_compare(zend_version(), '2.0.0-dev', '<')) die('skip ZendEngine 2 needed'); ?>
--FILE--
<?php

enum foo {
  bar = 6
}

class myClass {
}

$classList = get_declared_classes();
var_dump(in_array("myClass", $classList));
var_dump(in_array("foo", $classList));

?>
--EXPECTF--

bool(true)
bool(false)
