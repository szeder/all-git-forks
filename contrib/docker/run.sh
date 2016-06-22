#!/bin/sh

die() {
	echo "$@" >&2
	exit 1
}

build_debian() {
	cat >Dockerfile <<-EOF
	FROM debian:latest
	RUN apt-get update && \
		apt-get install -y libcurl4-gnutls-dev libexpat1-dev \
		gettext libz-dev libssl-dev build-essential
	RUN apt-get install -y locales
	COPY locale.gen /etc/locale.gen
	RUN locale-gen
	RUN groupadd -r $(id -gn) -g $(id -g) && \
		useradd -u $(id -u) -r -d "$HOME" -g $(id -g) -s /sbin/nologin $(id -un)
	USER $(id -un)
	EOF
	docker build -t $IMAGE .  || die "failed to build docker image"
}

DISTRO=debian
IMAGE=git-$DISTRO-$(id -un)
ROOT="$(realpath $(git rev-parse --show-cdup))"

test "$(docker images --format='{{.Repository}}' $IMAGE)" = $IMAGE || \
	build_$DISTRO
docker run -it --rm -v "$ROOT":"$ROOT" -w "$(pwd)" $IMAGE bash
