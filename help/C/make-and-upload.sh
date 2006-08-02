# This script is for George only, so recognize my machine :)
if [ ! -d /home/jirka/ ]; then
	echo "Only George wants to run this script (it's for updating the genius webpage)"
	exit
fi

echo rm -f *.html *.pdf
rm -f *.html *.pdf

echo docbook2html genius.xml
docbook2html genius.xml

echo docbook2pdf genius.xml
docbook2pdf genius.xml

echo scp *.html zinc.5z.com:/home/www/html/jirka/genius-documentation/
scp *.html zinc.5z.com:/home/www/html/jirka/genius-documentation/

echo scp genius.pdf zinc.5z.com:/home/www/html/jirka/genius-reference.pdf
scp genius.pdf zinc.5z.com:/home/www/html/jirka/genius-reference.pdf
