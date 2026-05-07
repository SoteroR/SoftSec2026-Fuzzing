#probably we need to change tto the afl
FROM aflplusplus/aflplusplus:stable

#just installing basic things
RUN apt-get update && apt-get install -y \
    build-essential \
    clang \
    make \
    wget \
    tar \
    pkg-config \
    sudo \
    && rm -rf /var/lib/apt/lists/*

#changing to temp directory
WORKDIR /tmp

#downloading libSDL
RUN wget https://github.com/libsdl-org/SDL/releases/download/release-2.30.2/SDL2-2.30.2.tar.gz && \
    tar -xzf SDL2-2.30.2.tar.gz

#compiles normally
RUN cd SDL2-2.30.2 && \
  ./configure --prefix=/app/compiled && \
    make && \
    make install && \
    ldconfig

#compiles with AFL instrumentation
RUN cd SDL2-2.30.2 && \
    CC=afl-clang-fast \
    CXX=afl-clang-fast++ \
    ./configure --disable-shared --prefix=/opt/sdl-afl && \
    make && \
    make install && \
    ldconfig


#change to app directory
WORKDIR /app
#copy our current project files inside
COPY . .
#starts a shell
CMD ["/bin/bash"]
