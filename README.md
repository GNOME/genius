# Genius Calculator

Genius calculator is a general purpose calculator and mathematics tool
with many features.

For a manual read [help/genius.txt](help/genius.txt) (or see the manual in the gnome
help browser).

Requirements:
  - lex (tested under flex)
  - yacc (tested under bison -y)
  - gmp (relatively new one required)
  - mpfr (relatively new one)
  - glib 2.12

And for the GNOME frontend you need:
  - gtk+ 2.18
  - vte
  - gtksourceview or gtksourceview2 (optional but recommended)

If you want to compile without the GNOME frontend, try the `--disable-gnome`
argument to the ./configure script.  You will miss out on the GUI stuff
(which includes the plotting) but you can use all the rest nicely.

It's under GPL so read COPYING

Note: the gtkextra/ directory which includes the plotting widgetry is
copyright: Adrian E. Feiguin <feiguin@ifir.edu.ar> and is under LGPL.  When
GtkExtra is actually released, stable, free of bugs, widely deployed and all
that it will become a requirement rather then being included like this.  This
seems very unlikely at this time.

George <jirka@5z.com>

<a href='https://flathub.org/apps/details/org.gnome.Genius'><img width='240' alt='Download on Flathub' src='https://flathub.org/assets/badges/flathub-badge-i-en.png'/></a>

## Useful links

- Homepage: <https://www.jirka.org/genius.html>
- Report issues: <https://gitlab.gnome.org/GNOME/genius/issues/>
- Donate: <https://www.gnome.org/friends/>
- Translate: <https://wiki.gnome.org/TranslationProject>

