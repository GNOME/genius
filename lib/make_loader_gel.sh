#!/bin/sh
echo "# Automatically generated loader, don't touch"
echo
for n in "$@"; do
	echo "load $n"
done

echo "ProtectAll()"
