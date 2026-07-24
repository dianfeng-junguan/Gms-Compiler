.PHONY: tool toolgen compiler all
all: tool compiler
compiler:
	make -C build
tool: toolgen
	build/gen.exe > debugutil.c
toolgen:
	make -C build gen
# 	g++ tool/gen.cpp -o build/gen -g
run:
	build/GMS_COMPILER -i build/test.gms -o build/a.asm
	nasm build/a.asm -o build/a.o -g -felf64
	gcc build/a.o -o build/a.out -e _start -g -nostartfiles -no-pie -fsanitize=address
runarm:
	build/GMS_COMPILER -i build/test.gms -o build/arm.asm -m aarch64
	as build/arm.asm -o build/arm.o -g
	gcc build/arm.o -o build/arm.out -e _start -g
lldb:
	lldb -- build/GMS_COMPILER -i build/test.gms -o build/arm.asm -m aarch64
gdb:
	gdb --args build/GMS_COMPILER -i build/test.gms -o build/a.asm -m amd64
