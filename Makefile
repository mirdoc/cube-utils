MAKEFLAGS += --quiet

TARGETS = cubepro-encoder cubex-encoder cube3-encoder cube-encoder cube-decoder
LIBS = -lm
CC = gcc
CFLAGS += -g -Wall

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

TESTFILES = $(wildcard tests/*.bfb)
CUBEPROTESTS = $(patsubst %.bfb, %.encoded.cubepro, $(TESTFILES)) $(patsubst %.bfb, %.decoded.cubepro.bfb, $(TESTFILES))
CUBEXTESTS = $(patsubst %.bfb, %.encoded.cubex, $(TESTFILES)) $(patsubst %.bfb, %.decoded.cubex.bfb, $(TESTFILES))
TESTOUTPUTS = $(CUBEPROTESTS) $(CUBEXTESTS)

.PHONY: default all clean test

all: $(TARGETS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGETS) $(OBJECTS)

cubepro-encoder: cube-encoder.o blowfish.o
	$(CC) $^ -Wall $(LIBS) -o $@

cubex-encoder: cubepro-encoder
	cp $^ $@

cube3-encoder: cubepro-encoder
	cp $^ $@

cube-encoder: cubepro-encoder
	cp $^ $@

cube-decoder: cube-decoder.o blowfish.o
	$(CC) $^ -Wall $(LIBS) -o $@

%.encoded.cubepro: %.bfb cubepro-encoder
	./cubepro-encoder $< $(patsubst %.bfb, %.encoded.cubepro, $<)
	diff $(patsubst %.bfb, %.cubepro, $<) $(patsubst %.bfb, %.encoded.cubepro, $<)

%.encoded.cubex: %.bfb cubex-encoder
	./cubex-encoder $< $(patsubst %.bfb, %.encoded.cubex, $<)
	diff $(patsubst %.bfb, %.cubex, $<) $(patsubst %.bfb, %.encoded.cubex, $<)

%.decoded.cubepro.bfb: %.encoded.cubepro cube-decoder
	./cube-decoder $< $(patsubst %.encoded.cubepro, %.decoded.cubepro.bfb, $<)
	diff $(patsubst %.encoded.cubepro, %.bfb, $<) $(patsubst %.encoded.cubepro, %.decoded.cubepro.bfb, $<)

%.decoded.cubex.bfb: %.encoded.cubex cube-decoder
	./cube-decoder $< $(patsubst %.encoded.cubex, %.decoded.cubex.bfb, $<)
	diff $(patsubst %.encoded.cubex, %.bfb, $<) $(patsubst %.encoded.cubex, %.decoded.cubex.bfb, $<)

test: test_clean $(TARGETS) $(TESTOUTPUTS)
	echo "All tests completed successfully"

test_clean:
	-rm -f tests/*coded.*

clean: test_clean
	-rm -f *.o
	-rm -f $(TARGETS)
