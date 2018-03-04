#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {

	int i;
	for (i = 0; i < 200; i++) {

		char filename[1000] = "file";
		char number [12];

		sprintf(number, "%d", i);

		strcat(filename, number);

		FILE * fp = fopen(filename, "w");


	}

}
