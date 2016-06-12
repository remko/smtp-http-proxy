IMAGE_NAME=smtp-http-proxy
APK_IMAGE_NAME=$(IMAGE_NAME)-apk

.DEFAULT: build
.PHONY: build
build:
	docker build -t $(IMAGE_NAME) $(CURDIR)
	docker run --rm=true -v $(CURDIR)/docker-build:/opt/build $(IMAGE_NAME) cp /opt/src/smtp-http-proxy /opt/build

.PHONY: apk
apk:
	-rm -rf docker-build/packages
	docker build -t $(APK_IMAGE_NAME) -f $(CURDIR)/Dockerfile.apk $(CURDIR)
	docker run --rm=true -v $(CURDIR)/docker-build:/opt/build $(APK_IMAGE_NAME) cp -r /home/build/packages /opt/build

.PHONY: sync-apk
apk-sync:
	rsync -avz docker-build/packages/alpine/ el-tramo.be:cdn/alpine/smtp-http-proxy/
