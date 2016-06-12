IMAGE_NAME=smtp-http-proxy
APK_IMAGE_NAME=$(IMAGE_NAME)-apk
DEBIAN_IMAGE_NAME=$(IMAGE_NAME)-debian

.DEFAULT: build
.PHONY: build
build:
	docker build -t $(IMAGE_NAME) $(CURDIR)
	docker run --rm=true -v $(CURDIR)/docker-build:/opt/build $(IMAGE_NAME) cp /opt/src/smtp-http-proxy /opt/build

.PHONY: apk
apk:
	-rm -rf docker-build/apk
	mkdir -p docker-build/apk
	docker build -t $(APK_IMAGE_NAME) -f $(CURDIR)/Dockerfile.apk $(CURDIR)
	docker run --rm=true -v $(CURDIR)/docker-build:/opt/build $(APK_IMAGE_NAME) cp -r /home/build/packages /opt/build/apk

.PHONY: apk-deploy
apk-deploy:
	rsync -avz --delete docker-build/apk/packages/alpine/ el-tramo.be:cdn/alpine/smtp-http-proxy/

.PHONY: debian
debian:
	-rm -rf docker-build/debian
	mkdir -p docker-build/debian
	docker build -t $(DEBIAN_IMAGE_NAME) -f $(CURDIR)/Dockerfile.debian $(CURDIR)
	# docker run --rm=true -v $(CURDIR)/docker-build:/opt/build $(DEBIAN_IMAGE_NAME) cp -r /home/build/packages /opt/build/debian
