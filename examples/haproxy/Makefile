DOCKER_IP=$(shell echo $$DOCKER_HOST | sed 's/tcp:\/\///g' | sed 's/:.*//')

.PHONY: docker
docker: 
	docker build -t haproxy-example .

.PHONY: docker-run
docker-run:
	@echo "Starting on http://$(DOCKER_IP):9080"
	docker run -it --rm=true -p 9080:80 -e AWS_LAMBDA_API_KEY=$(AWS_LAMBDA_API_KEY) --name haproxy-example haproxy-example $(DOCKER_COMMAND)

