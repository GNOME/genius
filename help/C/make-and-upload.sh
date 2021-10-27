#!/bin/sh

# This script is for George only, so recognize my machine :)
if [ ! -d /home/jirka/ ]; then
	echo "Only George wants to run this script (it's for updating the genius webpage)"
	exit
fi

./make-only.sh

echo scp *.html zinc.5z.com:/home/www/html/jirka/genius-documentation/
scp *.html zinc.5z.com:/home/www/html/jirka/genius-documentation/

echo scp figures/*.png zinc.5z.com:/home/www/html/jirka/genius-documentation/figures/
scp figures/*.png zinc.5z.com:/home/www/html/jirka/genius-documentation/figures/

echo
echo FIXME: PDF version is not working currently

#echo scp genius.pdf zinc.5z.com:/home/www/html/jirka/genius-reference.pdf
#scp genius.pdf zinc.5z.com:/home/www/html/jirka/genius-reference.pdf
