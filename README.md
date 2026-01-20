# Gamessis Compiler

Gamessis Compiler is a toy compiler i wrote for fun. The language is called Gamessis(/dƷεm'm𝐈s𝐈z/, I pronounce it like that). The grammer is based on C and modified a bit to meet my own taste.

The Compiler outputs nasm file now.

# Gamessis Grammar

Gamessis is case-sensitive.

## Keywords

- let
- fn
- if
- else
- while
- extern
- break
- return
- int
- string

## Types

There are two types in Gamessis now: int and string. Int is always 8 bytes(on 64bit machine, 4bytes on 32bit). String is actually a pointer like char* in C.

## Variable Definition
```
let name=value;
```
The compiler will infer the type of the varaible.

## Function Definition
```
fn name(argname argtype,...):returntype{
	...
}
```
## Extern Variable and Function Declaration
```
extern let name:type;

extern fn name(argname argtype,...):returntype;
```
## Function Call

```c
func(arg1,arg2,...);
```



## If, Else-if, Else and While

```
if condition {
}else if condition{
}else{
}

while condition {
	break;
}
```
## Return
```
return value;

return;
```

## Operators

Most operators are the same as C. 

Supported operators:

- plus(+), minus(-), multiply(*), divide(/), mod(%)
- parenthesis (( and ))
- comma(,), it returns the value of the rightest expression
- assign(=)
- equal(==), greater(>), less(<), greater equal(>=), less equal(<=), not equal(!=)

# Template Gamessis Code

```c
extern fn puts(cont string):int;
extern fn putchar(cont int):int;
extern fn exit(exitcode int):int;

fn main():int{
   puts("fibonacci");
   let a=1;
   let b=1;
   let c=0;
   let i=0;
   putchar(48+a);
   putchar(10);
   putchar(48+b);
   putchar(10);
   while i<10 {
   	 c=a+b;
	 putchar((c/100)+48);
	 putchar((c%100/10)+48);
	 putchar((c%10)+48);
	 putchar(10);
	 a=b;
	 b=c;
	 i=i+1;
   }  
   exit(0);
   return 0;
}
```

The puts, putchar and exit functions are actually from standard C library. 

# Compile

run

```bash
cmake -B bin
```
at project root directory and then run

```bash
make -C bin
```

to compile.

# Usage
```
GMS_COMPILER input_file output_file
```
output\_file is optional. If no output\_file is given, the compiler will assume the name as a.asm.

**reminder:** if your nasm complains 'label-redef-late' error, add

```bash
-wno-label-redef-late
```

when compiling with nasm. And do not use PIE and startfiles as it might cause problems when compiling with gcc.
