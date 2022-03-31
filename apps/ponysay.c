/**
 * @brief Wrapper tool to launch ponysay
 *
 * @warning This is a silly workaround for toolchain limitations.
 *
 * @copyright None asserted.
 */
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char * argv[]) {
	char ** args = malloc(sizeof(char *) * (argc + 3)); /* two for -m ponysay, one for null */

	setenv("KUROKOPATH","/usr/ponysay/",1);

	args[0] = "kuroko";
	args[1] = "-m";
	args[2] = "ponysay";

	for (int i = 1; i < argc+1; ++i) {
		args[i+2] = argv[i];
	}

	return execvp("kuroko",args);
}
