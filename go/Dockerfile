FROM golang:1.10-stretch

WORKDIR /go
COPY main.go /go/
RUN go build /go/main.go

ENTRYPOINT ["/go/main"]
