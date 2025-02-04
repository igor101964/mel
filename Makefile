mel: mel.c
	$(CC) mel.c -o mel -std=c99 -lcurl -ljson-c

debug: mel.c
	$(CC) mel.c -o mel -Wall -Wextra -pedantic -std=c99 -lcurl -lcjson-c -g

install: mel
	sudo cp mel /usr/local/bin/
	sudo chmod +x /usr/local/bin/mel
