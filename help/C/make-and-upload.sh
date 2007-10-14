# This script is for George only, so recognize my machine :)
if [ ! -d /home/jirka/ ]; then
	echo "Only George wants to run this script (it's for updating the genius webpage)"
	exit
fi

echo rm -f *.html *.pdf
rm -f *.html *.pdf

echo SP_ENCODING=\"utf-8\" docbook2html genius.xml
SP_ENCODING="utf-8" docbook2html genius.xml

echo docbook2ps genius.xml
docbook2ps genius.xml
echo ps2pdf genius.ps genius.pdf
ps2pdf genius.ps genius.pdf

echo scp *.html zinc.5z.com:/home/www/html/jirka/genius-documentation/
scp *.html zinc.5z.com:/home/www/html/jirka/genius-documentation/

echo scp -r figures zinc.5z.com:/home/www/html/jirka/genius-documentation/
scp -r figures zinc.5z.com:/home/www/html/jirka/genius-documentation/

echo scp genius.pdf zinc.5z.com:/home/www/html/jirka/genius-reference.pdf
scp genius.pdf zinc.5z.com:/home/www/html/jirka/genius-reference.pdf
