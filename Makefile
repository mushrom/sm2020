# TODO:
SRC  = $(wildcard src/*.cpp)
OBJ  = $(SRC:.cpp=.o)
DEPS = $(SRC:.cpp=.d)

CXXFLAGS += -std=c++17 -Wall -O2 -I./include
CXXFLAGS += $(shell ./env/bin/grend-config --cflags --libs)

testthing: $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $@

testthing.html: $(OBJ) shell.html
	$(CXX) $(OBJ) env/lib/libgrend.a grend/libs/nanovg/build/libnanovg.a \
		$(CXXFLAGS) -s USE_SDL_TTF=2 -s USE_FREETYPE=1 -o $@ \
		--preload-file env/share@/share \
		--preload-file env/assets@/assets \
		--preload-file env/save.map@/save.map \
		--shell-file ./shell.html

$(OBJ): env/lib/libgrend.a

.PHONY: test
test: testthing
	./testthing

.PHONY: html
html: testthing.html

.PHONY: clean
clean:
	-rm $(OBJ)
	-rm -f testthing testthing.html
