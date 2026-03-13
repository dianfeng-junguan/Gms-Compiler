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
	nasm bin/a.asm -o bin/a.o -g -felf64
	gcc bin/a.o -o bin/a.out -e _start -g -nostartfiles -no-pie -fsanitize=address
runarm:
	bin/GMS_COMPILER -i bin/test.gms -o bin/arm.asm -m aarch64
	as bin/arm.asm -o bin/arm.o -g
	gcc bin/arm.o -o bin/arm.out -e _start -g
lldb:
	lldb -- bin/GMS_COMPILER -i bin/test.gms -o bin/arm.asm -m aarch64
gdb:
	gdb --args bin/GMS_COMPILER -i bin/test.gms -o bin/a.asm -m amd64
