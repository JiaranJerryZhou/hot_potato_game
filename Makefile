CPPFLAGS=-ggdb3 -Wall -Werror -pedantic -std=gnu++11
TARGETS=ringmaster player

all: $(TARGETS)
clean:
	rm -f $(TARGETS) *~

ringmaster: ringmaster.cpp
	g++ $(CPPFLAGS) -g -o $@ $<

player: player.cpp
	g++ $(CPPFLAGS) -g -o $@ $<
