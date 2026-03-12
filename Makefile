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
runarm:
	bin/GMS_COMPILER -i bin/test.gms -o bin/arm.asm -m aarch64
	as bin/arm.asm -o bin/arm.o -g
	gcc bin/arm.o -o bin/arm.out -e _start -g
lldb:
	lldb -- bin/GMS_COMPILER -i bin/test.gms -o bin/arm.asm -m aarch64
