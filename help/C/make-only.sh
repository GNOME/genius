#!/bin/sh

# This script is for George only, so recognize my machine :)
if [ ! -d /home/jirka/ ]; then
	echo "Only George wants to run this script (it's for updating the genius webpage)"
	exit
fi

echo rm -f *.html *.pdf *.ps
rm -f *.html *.pdf *.ps

#echo SP_ENCODING=\"utf-8\" docbook2html genius.xml
#SP_ENCODING="utf-8" docbook2html genius.xml

echo xmlto html genius.xml
xmlto html genius.xml

#echo xmlto pdf genius.xml
#xmlto pdf genius.xml

#echo docbook2pdf genius.xml
#docbook2pdf genius.xml
#echo dblatex genius.xml

echo
echo
echo
echo
echo FIXME: dblatex seems broken, we should fix this to build a pdf version again
echo
echo
echo
echo

#dblatex genius.xml
#echo docbook2ps genius.xml

#docbook2ps genius.xml
#echo ps2pdf genius.ps genius.pdf
#ps2pdf genius.ps genius.pdf
