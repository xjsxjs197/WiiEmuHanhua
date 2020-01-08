.PHONY = all wii wii-clean wii-run

all: wii

clean: wii-clean

run: wii-run

wii:
	$(MAKE) -f Makefile.wii

wii-clean:
	$(MAKE) -f Makefile.wii clean

wii-run: wii
	$(MAKE) -f Makefile.wii run
