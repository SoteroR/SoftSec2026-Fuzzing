IMAGE_NAME  := group-17-fuzz
CONTAINER   := group-17

.PHONY: build run fuzz shell clean

build:
	docker build -t $(IMAGE_NAME) .

## Drop into the container interactively
run:
	docker run -ti --rm --privileged -v "$$PWD":/app --name $(CONTAINER) $(IMAGE_NAME)

fuzz-whitebox:
	afl-clang-fast src/harness.c \
		-fsanitize=address \
		-I/opt/sdl-afl/include \
		-L/opt/sdl-afl/lib \
		-o target-afl \
		-lSDL2 -lm
	afl-fuzz -i seeds -o findings -- ./target-afl @@



compile-harness-afl:
	AFL_USE_ASAN=1 afl-clang-fast \
		-O1 -g \
		src/harness.c \
		-I/opt/sdl-afl/include \
		-L/opt/sdl-afl/lib \
		-o target-afl \
		-lSDL2 -lm -ldl -lpthread



compile-harness-normal:
	clang src/harness.c \
      -I/opt/sdl-normal/include \
      -L/opt/sdl-normal/lib \
      -lSDL2 \
      -o target-normal


## Grey-box fuzzing: coverage-guided (instrumented binary)
fuzz:
	docker run -ti --rm --name $(CONTAINER) $(IMAGE_NAME) \
	  afl-fuzz -i seeds -o findings -s 123 -- main @@



## Remove containers and image
clean:
	-docker rm -f $(CONTAINER) $(CONTAINER)-blind
	-docker rmi -f $(IMAGE_NAME)
