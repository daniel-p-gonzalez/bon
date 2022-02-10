FROM ubuntu:18.04

WORKDIR /bon_build

RUN apt-get update && apt-get -y install vim cmake clang-4.0 clang-4.0-dev llvm-4.0 llvm-4.0-dev zlib1g-dev gcc g++
# ENV CXX=clang++-4.0
# ENV CC=clang-4.0
ENV CXX=g++
ENV CC=gcc

COPY src src
COPY build.sh build.sh
# COPY benchmarks benchmarks
COPY examples examples
COPY stdlib stdlib

RUN bash ./build.sh
