--TEST--
Enum: an enum shouldn't be empty
--SKIPIF--
<?php if (version_compare(zend_version(), '2.0.0-dev', '<')) die('skip ZendEngine 2 needed'); ?>
--FILE--
<?php

enum foo {
}

echo "Done\n"; //Shouldn't be displayed
?>
--EXPECTF--

Parse error: syntax error, unexpected '}', expecting identifier (T_STRING) in %s on line %d
