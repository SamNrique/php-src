--TEST--
Enum: an enum arg shouldn't be filled with a class instance
--SKIPIF--
<?php if (version_compare(zend_version(), '2.0.0-dev', '<')) die('skip ZendEngine 2 needed'); ?>
--FILE--
<?php

ini_set('display_error', 1);
error_reporting(E_NOTICE|E_ALL);

enum myEnum {
  foo = 3, 
  bar = 4 
}

class myClass {
}

function myFunction(myEnum $arg1) {
  echo $arg1;  // shouldn't be displayed
}

myFunction(new myClass());  //Should fail here

echo "Done\n"; // shouldn't be displayed
?>
--EXPECTF--

Catchable fatal error: Argument 1 passed to myFunction() must be a value of enum myEnum, instance of myClass given, called in %s on line %d and defined in %s on line %d
