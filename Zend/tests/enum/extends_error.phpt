--TEST--
Enum: an enum should'nt be inheritable
--SKIPIF--
<?php if (version_compare(zend_version(), '2.0.0-dev', '<')) die('skip ZendEngine 2 needed'); ?>
--FILE--
<?php

enum foo {
	bar = 6
}

class myClass extends foo {
}

echo "Done\n"; // shouldn't be displayed
?>
--EXPECTF--

Fatal error: Class myClass cannot extend from enum foo in %s on line %d
