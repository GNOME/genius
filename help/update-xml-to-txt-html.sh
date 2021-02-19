#!/bin/sh
LANGS="cs de el es fr pt_BR ru sv"

echo
echo Running: xmlto -o C/html/ html C/genius.xml
xmlto -o C/html/ html C/genius.xml || exit 1

echo
echo Running: docbook2txt C/genius.xml
docbook2txt C/genius.xml || exit 1

echo
echo Running: dos2unix genius.txt
dos2unix genius.txt || exit 1

echo
echo "Running: itstool -o genius.pot C/genius.xml"
itstool -o genius.pot C/genius.xml || exit 1

for n in $LANGS ; do
	echo
	echo "Running: msgmerge -o $n/new.po $n/$n.po genius.pot"
	msgmerge -o $n/new.po $n/$n.po genius.pot || exit 1

	echo
	echo "Running: mv -f $n/new.po $n/$n.po"
	mv -f $n/new.po $n/$n.po || exit 1

	echo
	echo "Running: msgfmt -o messages.mo $n/$n.po"
	msgfmt -o messages.mo $n/$n.po || exit 1

	echo
	echo "Running: itstool -m messages.mo -o $n/ C/genius.xml"
	itstool -m messages.mo -o $n/ C/genius.xml || exit 1

	echo
	echo "Running: rm -f messages.mo"
	rm -f messages.mo

	echo
	echo Running rm -f $n/html/*.html
	rm -f $n/html/*.html || exit 1

	echo
	echo Running xmlto -o $n/html/ html $n/genius.xml
	xmlto -o $n/html/ html $n/genius.xml || exit 1
done

echo
echo "Running: rm -f messages.mo genius.pot"
rm -f messages.mo genius.pot

echo
echo Running: make-makefile-am.sh
./make-makefile-am.sh

echo
echo Now you should rerun automake I suppose ...
