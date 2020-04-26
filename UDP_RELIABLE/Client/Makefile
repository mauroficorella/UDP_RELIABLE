client: clientUDP.o ../common_functions.o ../client_functions.o
	gcc -Wall -Wextra -o clientUDP clientUDP.o ../client_functions.o ../common_functions.o
	./clientUDP

../common_functions.o: ../common_functions.c ../common_functions.h ../config.h
	gcc -Wall -Wextra -o ../common_functions.o ../common_functions.c -c

../client_functions.o: ../client_functions.c ../client_functions.h ../config.h
	gcc -Wall -Wextra -o ../client_functions.o ../client_functions.c -c