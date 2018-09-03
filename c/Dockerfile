FROM golang:1.10-stretch

RUN mkdir /c
COPY main.c /c/
RUN gcc -o /c/main -Wall -Werror /c/main.c

ENTRYPOINT ["/c/main"]
