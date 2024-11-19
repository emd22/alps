#include <stdio.h>


int main() {
	char *s = "Hello, %s %lu\n";
	char *n = "Ethan";
	long d = 10;
	printf(s, n, d);

	return 0;
}
