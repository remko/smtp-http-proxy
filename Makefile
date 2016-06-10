IMAGE_NAME=smtp-http-proxy
APK_IMAGE_NAME=$(IMAGE_NAME)-apk

.DEFAULT: build
build:
	docker build -t $(IMAGE_NAME) $(CURDIR)
	docker run --rm=true -v $(CURDIR)/docker-build:/opt/build $(IMAGE_NAME) cp /opt/src/smtp-http-proxy /opt/build

.PHONY: apk
apk:
	docker build -t $(APK_IMAGE_NAME) -f $(CURDIR)/Dockerfile.apk $(CURDIR)
	docker run --rm=true -v $(CURDIR)/docker-build:/opt/build $(APK_IMAGE_NAME) cp -r /home/build/packages /opt/build
