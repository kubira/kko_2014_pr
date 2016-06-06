################################################################################
#  Soubor:   Makefile                                                          #
#  Autor:    Radim Kubiš, xkubis03                                             #
#  Vytvořen: 8. březen 2014                                                    #
#                                                                              #
#  Projekt do předmětu Kódování a komprese (KKO) 2014                          #
#                                                                              #
#                    KONVERZE OBRAZOVÉHO FORMÁTU GIF NA BMP                    #
#                   ----------------------------------------                   #
#                                                                              #
#  Soubor  Makefile   pro  překlad  demostrační  aplikace  knihovny  gif2bmp.  #
#                                                                              #
################################################################################

# Návěští pro překlad programu s knihovnou gif2bmp a matematickou knihovnou
all:
	gcc -std=c99 gif2bmp.c main.c -o gif2bmp -lm -g -pedantic

# Návěští pro smazání souborů vytvořených při překladu
clean:
	rm -f gif2bmp
