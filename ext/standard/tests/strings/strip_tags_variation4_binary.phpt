--TEST--
Test strip_tags() function : usage variations - invalid values for 'str' and valid 'allowable_tags', binary variant
--INI--
short_open_tag = on
--FILE--
<?php
/* Prototype  : string strip_tags(string $str [, string $allowable_tags])
 * Description: Strips HTML and PHP tags from a string
 * Source code: ext/standard/string.c
*/

/*
 * testing functionality of strip_tags() by giving invalid values for $str and valid values for $allowable_tags argument
*/

echo "*** Testing strip_tags() : usage variations ***\n";

// unexpected values for $string
$strings = array (
  b"<abc>hello</abc> \t\tworld... <ppp>strip_tags_test</ppp>",
  b'<abc>hello</abc> \t\tworld... <ppp>strip_tags_test</ppp>',
  b"<%?php hello\t world?%>",
  b'<%?php hello\t world?%>',
  b"<<htmL>>hello<</htmL>>",
  b'<<htmL>>hello<</htmL>>',
  b"<a.>HtMl text</.a>",
  b'<a.>HtMl text</.a>',
  b"<nnn>I am not a valid html text</nnn>",
  b'<nnn>I am not a valid html text</nnn>',
  b"<nnn>I am a quoted (\") string with special chars like \$,\!,\@,\%,\&</nnn>",
  b'<nnn>I am a quoted (\") string with special chars like \$,\!,\@,\%,\&</nnn>',
);

//valid html and php tags
$quotes = b"<p><a><?php<html>";

//loop through the various elements of strings array to test strip_tags() functionality
$iterator = 1;
foreach($strings as $string_value)
{
      echo "-- Iteration $iterator --\n";
      var_dump( strip_tags($string_value, $quotes) );
      $iterator++;
}

echo "Done";
?>
--EXPECT--
*** Testing strip_tags() : usage variations ***
-- Iteration 1 --
string(32) "hello 		world... strip_tags_test"
-- Iteration 2 --
string(34) "hello \t\tworld... strip_tags_test"
-- Iteration 3 --
string(0) ""
-- Iteration 4 --
string(0) ""
-- Iteration 5 --
string(18) "<htmL>hello</htmL>"
-- Iteration 6 --
string(18) "<htmL>hello</htmL>"
-- Iteration 7 --
string(9) "HtMl text"
-- Iteration 8 --
string(9) "HtMl text"
-- Iteration 9 --
string(26) "I am not a valid html text"
-- Iteration 10 --
string(26) "I am not a valid html text"
-- Iteration 11 --
string(62) "I am a quoted (") string with special chars like $,\!,\@,\%,\&"
-- Iteration 12 --
string(64) "I am a quoted (\") string with special chars like \$,\!,\@,\%,\&"
Done
