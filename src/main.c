#include <stdio.h>
#include <stdint.h>

#include "rom.h"
#include "cpu.h"

int main(int argc, char** argv) {
	if(argc < 2) {
		printf("usage: %s romPath\n", argv[0]);
		return 1;
	}

	if(loadROM(argv[1]) != 0) {
		return 1;
	}

	cpuInit();
	while(cpuStep() == 0) {};
	
	return 0;
}
