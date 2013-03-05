#!/bin/bash -e

# ensure sudo permission is up-to-date
sudo git status
git pull upstream master && \
	make && \
	make doc html && \
	sudo make prefix=/usr/local install install-doc install-html

