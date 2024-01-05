.PHONY:	rebuild

rebuild:
	autoreconf --install
	sed --in-place=~ -e 's/\t-rm -f Makefile/\techo "include rebuild.mk" > Makefile/' Makefile.in
	./configure
	make clean
	make all

