--TEST--
Passing configuration file through tidy_parse_file()
--SKIPIF--
<?php if (!extension_loaded("tidy")) print "skip"; ?>
--FILE--
<?php
	$path = dirname(__FILE__);
        $tidy = tidy_parse_file("{$path}/016.html", "{$path}/016.tcfg");
    	tidy_clean_repair($tidy);
        echo tidy_get_output($tidy);
?>
--EXPECT--
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 3.2//EN">
<html>
<head>
<title></title>

<style type="text/css">
 p.c1 {font-weight: bold}
</style>
</head>
<body>
<p class="c1">testing</p>
</body>
</html>
