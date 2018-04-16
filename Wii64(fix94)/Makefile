.PHONY = all wii wii-clean wii-run

all: wii

run: wii-run

clean: wii-clean

wii:
	$(MAKE) -f Makefile.glN64_wii
#	$(MAKE) -f Makefile.sft_gfx_wii

wii-clean:
	$(MAKE) -f Makefile.glN64_wii clean
#	$(MAKE) -f Makefile.sft_gfx_wii clean

wii-run: wii
	$(MAKE) -f Makefile.glN64_wii run

