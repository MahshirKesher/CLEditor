st: source.c
	$(CC) source.c -o cle -Wall -Wextra -pedantic -std=c99
	$(CC) remaster.c -g -o rem -Wall -Wextra -pedantic -std=c99
