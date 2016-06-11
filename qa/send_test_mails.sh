#!/bin/bash

echo -e 'Subject: Test 1\n\nThis is a test' | msmtp \
		-d \
		--host=127.0.0.1 --port=8025 \
		--from=sender@example.com \
		receiver@example.com
echo -e 'Subject: Test 2\n\nThis is another test' | msmtp \
		-d \
		--host=127.0.0.1 --port=8025 \
		--from=sender@example.com \
		receiver@example.com
