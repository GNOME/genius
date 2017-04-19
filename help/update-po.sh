#!/bin/sh
if test "x$1" = "x" ; then
	echo USAGE: ./update-po.sh LANG
	exit 1
fi 
echo Running xml2po -u $1/$1.po C/genius.xml
xml2po -u $1/$1.po C/genius.xml
