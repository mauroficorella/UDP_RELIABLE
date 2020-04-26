Server: serverUDP.o ../common_functions.o ../server_functions.o
	gcc -Wall -Wextra -o serverUDP serverUDP.o ../server_functions.o ../common_functions.o
	./serverUDP

../common_functions.o: ../common_functions.c ../common_functions.h ../config.h
	gcc -Wall -Wextra -o ../common_functions.o ../common_functions.c -c

../server_functions.o: ../server_functions.c ../server_functions.h ../config.h
	gcc -Wall -Wextra -o ../server_functions.o ../server_functions.c -c
