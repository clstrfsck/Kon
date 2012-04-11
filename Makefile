
Kon.exe:	Kon.c
	gcc -Wall -nostdlib -Wl,-e,_entryPoint@0 -o $@ $? -lkernel32
