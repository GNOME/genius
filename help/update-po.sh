#!/bin/sh
if test "x$1" = "x" ; then
	echo USAGE: ./update-po.sh LANG
	exit 1
fi 
LL="$1"
#echo Running xml2po -u $1/$1.po C/genius.xml
#xml2po -u $1/$1.po C/genius.xml

echo
echo "Running: itstool -o genius.pot C/genius.xml"
itstool -o genius.pot C/genius.xml || exit 1

echo
echo "Running: msgmerge -o $LL/new.po $LL/$LL.po genius.pot"
msgmerge -o $LL/new.po $LL/$LL.po genius.pot || exit 1

echo
echo "Running: mv -f $LL/new.po $LL/$LL.po"
mv -f $LL/new.po $LL/$LL.po || exit 1

echo
echo "Running: rm -f genius.pot"
rm -f genius.pot
