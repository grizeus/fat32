make:
	gcc -g -o format ../formatter.c
	gcc -g -o create ../creator.c
	gcc -g -o reader ../reader.c
	# gcc -g -o dir ../dir_reader.c
	# gcc -g -o mkdir ../mkdir.c ../bootsec.c
	gcc -g -o fat ../main.c ../change_dir.c ../directory.c ../bootsec.c ../dir_reader.c ../mkdir.c
