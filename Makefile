make:
	gcc -g -o format formatter.c bootsec.c
	gcc -g -o creader reader.c bootsec.c
