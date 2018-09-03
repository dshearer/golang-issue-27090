TAG=issue-27090-c

docker build -t "${TAG}" . && docker run "${TAG}"
