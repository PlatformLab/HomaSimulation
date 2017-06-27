TESTING=0
all: checkmakefiles
	cd src && $(MAKE)
	cd ../inet && $(MAKE)

clean: checkmakefiles
	cd src && $(MAKE) clean

cleanall: checkmakefiles
	cd src && $(MAKE) MODE=release clean
	cd src && $(MAKE) MODE=debug clean
	rm -f src/Makefile

makefiles:
	cd src && opp_makemake -f --deep -X dcntopo/results -I../../inet/src -L../../inet/out/$$\(CONFIGNAME\)/src -lINET -DTESTING=$$\(TESTING\) -KTESTING=$(TESTING) -DINET_IMPORT -KINET_PROJ=../../inet

checkmakefiles:
	@if [ ! -f src/Makefile ]; then \
	echo; \
	echo '======================================================================='; \
	echo 'src/Makefile does not exist. Please use "make makefiles" to generate it!'; \
	echo '======================================================================='; \
	echo; \
	exit 1; \
	fi
