TAG=issue-27090-go

docker build -t "${TAG}" . && docker run "${TAG}"
