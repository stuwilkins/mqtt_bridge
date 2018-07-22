CC=gcc
CFLAGS=-I.
LDFLAGS=-lconfig -lpaho-mqtt3a
DEPS=mqtt_bridge.h
src = $(wildcard *.c)
obj = $(src:.c=.o)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

mqtt_bridge : $(obj)
	$(CC) -o $@ $^ $(LDFLAGS)


.PHONY: clean
clean:
	rm -f $(obj) mqtt_bridge
