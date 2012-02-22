--TEST--
Enum: an enum should'nt be used like trait
--SKIPIF--
<?php if (version_compare(zend_version(), '2.0.0-dev', '<')) die('skip ZendEngine 2 needed'); ?>
--FILE--
<?php

enum foo {
	bar = 6
}

class myClass {
  use foo;
}

echo "Done\n"; // shouldn't be displayed
?>
--EXPECTF--

Fatal error: myClass cannot use foo - it is not a trait in %s on line %d
