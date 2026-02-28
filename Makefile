.PHONY: tool toolgen compiler all
all: tool compiler
compiler:
	make -C bin
tool: toolgen
	bin/gen > debugutil.c
toolgen:
	gcc tool/gen.c -o bin/gen -g
run:
	bin/GMS_COMPILER -i bin/test.gms -o bin/a.asm
