#include <stdio.h>


int main() {

	FILE * file = fopen("rtt.c", "r");

	char c;
	while ((c = fgetc(file)) != EOF) {
		printf("%c", c);
	}

}
