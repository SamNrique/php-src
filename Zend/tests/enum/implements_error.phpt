--TEST--
Enum: an enum should'nt be implemented
--SKIPIF--
<?php if (version_compare(zend_version(), '2.0.0-dev', '<')) die('skip ZendEngine 2 needed'); ?>
--FILE--
<?php

enum foo {
	bar = 6
}

class myClass implements foo {
}

echo "Done\n"; // shouldn't be displayed
?>
--EXPECTF--

Fatal error: myClass cannot implement foo - it is not an interface in %s on line %d
