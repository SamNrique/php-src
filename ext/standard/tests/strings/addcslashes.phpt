--TEST--
Test addcslashes() function
--INI--
precision=14
--FILE--
<?php
/* Prototype: string addcslashes ( string $str, string $charlist );
   Description: Quote string with slashes in a C style.
                Returns a string with backslashes before characters that 
                are listed in charlist parameter.
*/

echo "*** Testing addcslashes() for basic operations ***\n";
/* checking normal operation of addcslashes */
$string = "goodyear12345NULL\0truefalse\a\v\f\b\n\r\t";
$charlist = array ( 
  NULL,
  2,
  array(5,6,7),
  "a",
  "\0",
  "\n",
  "\r",
  "\t",
  "\a",
  "\v",
  "\b",
  "\f"
);
/* loop prints string with backslashes before characters
   mentioned in $char using addcslashes() */
$counter = 1;
foreach($charlist as $char) {
  echo "-- Iteration $counter --\n";
  var_dump( addcslashes($string, $char) );
  $counter++;
}

/* charlist "\0..\37" would escape all characters with ASCII code between 0 and 31 */
echo "\n*** Testing addcslashes() with ASCII code between 0 and 31 ***\n";
var_dump( addcslashes($string, "\0..\37") );

/* Checking OBJECTS type */
echo "\n*** Testing addcslashes() with objects ***\n";
class string1
{
  public function __toString() {
    return "Object";
  }
}
$obj = new string1;
var_dump( addcslashes($obj, "b") );

/* Miscelleneous input */
echo "\n*** Testing addcslashes() with miscelleneous input arguments ***\n";
var_dump( addcslashes("", "") );
var_dump( addcslashes("", "burp") );
var_dump( addcslashes("kaboemkara!", "") );
var_dump( addcslashes("foobarbaz", 'bar') );
var_dump( addcslashes('foo[ ]', 'A..z') );
var_dump( @addcslashes("zoo['.']", 'z..A') );
var_dump( addcslashes('abcdefghijklmnopqrstuvwxyz', "a\145..\160z") );
var_dump( addcslashes( 123, 123 ) );
var_dump( addcslashes( 123, NULL) );
var_dump( addcslashes( NULL, 123) );
var_dump( addcslashes( 0, 0 ) );
var_dump( addcslashes( "\0" , 0 ) );
var_dump( addcslashes( NULL, NULL) );
var_dump( addcslashes( -1.234578, 3 ) );
var_dump( addcslashes( " ", " ") );
var_dump( addcslashes( "string\x00with\x00NULL", "\0") );

echo "\n*** Testing error conditions ***\n";
/* zero argument */
var_dump( addcslashes() );

/* unexpected arguments */
var_dump( addcslashes("foo[]") );
var_dump( addcslashes('foo[]', "o", "foo") );

echo "Done\n"; 

?>
--EXPECTF--
*** Testing addcslashes() for basic operations ***
-- Iteration 1 --
string(38) "goodyear12345NULL truefalse\a\v\f\b

	"
-- Iteration 2 --
string(39) "goodyear1\2345NULL truefalse\a\v\f\b

	"
-- Iteration 3 --

Notice: Array to string conversion in %s on line %d
string(44) "good\ye\a\r12345NULL t\ruef\alse\\a\v\f\b

	"
-- Iteration 4 --
string(41) "goodye\ar12345NULL truef\alse\\a\v\f\b

	"
-- Iteration 5 --
string(41) "goodyear12345NULL\000truefalse\a\v\f\b

	"
-- Iteration 6 --
string(39) "goodyear12345NULL truefalse\a\v\f\b\n
	"
-- Iteration 7 --
string(39) "goodyear12345NULL truefalse\a\v\f\b
\r	"
-- Iteration 8 --
string(39) "goodyear12345NULL truefalse\a\v\f\b

\t"
-- Iteration 9 --
string(45) "goodye\ar12345NULL truef\alse\\\a\\v\\f\\b

	"
-- Iteration 10 --
string(43) "goodyear12345NULL truefalse\\a\\\v\\f\\b

	"
-- Iteration 11 --
string(43) "goodyear12345NULL truefalse\\a\\v\\f\\\b

	"
-- Iteration 12 --
string(44) "goodyear12345NULL true\false\\a\\v\\\f\\b

	"

*** Testing addcslashes() with ASCII code between 0 and 31 ***
string(44) "goodyear12345NULL\000truefalse\a\v\f\b\n\r\t"

*** Testing addcslashes() with objects ***
string(7) "O\bject"

*** Testing addcslashes() with miscelleneous input arguments ***
string(0) ""
string(0) ""
string(11) "kaboemkara!"
string(14) "foo\b\a\r\b\az"
string(11) "\f\o\o\[ \]"
string(10) "\zoo['\.']"
string(40) "\abcd\e\f\g\h\i\j\k\l\m\n\o\pqrstuvwxy\z"
string(6) "\1\2\3"
string(3) "123"
string(0) ""
string(2) "\0"
string(1) " "
string(0) ""
string(10) "-1.2\34578"
string(2) "\ "
string(22) "string\000with\000NULL"

*** Testing error conditions ***

Warning: Wrong parameter count for addcslashes() in %s on line %d
NULL

Warning: Wrong parameter count for addcslashes() in %s on line %d
NULL

Warning: Wrong parameter count for addcslashes() in %s on line %d
NULL
Done
