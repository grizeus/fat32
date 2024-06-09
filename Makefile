make:
	# gcc -g -o format ../formatter.c
	gcc -g -o reader ../reader.c
	gcc -g -o fat ../main.c ../creator.c ../formatter.c ../change_dir.c ../directory.c ../bootsec.c ../dir_reader.c ../mkdir.c ../touch.c ../utility.c
