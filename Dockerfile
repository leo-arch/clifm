# Replace latest with a pinned version tag from https://hub.docker.com/_/alpine
#
# We suggest using the major.minor tag, not major.minor.patch.
FROM archlinux:latest

# Non-root user for security purposes.
#
# UIDs below 10,000 are a security risk, as a container breakout could result
# in the container being ran as a more privileged user on the host kernel with
# the same UID.
#
# Static GID/UID is also useful for chown'ing files outside the container where
# such a user does not exist.
#RUN useradd -u 10001 -m -G wheel -s /bin/bash leoarch

# Install packages here with `apk add --no-cache`, copy your binary
# into /sbin/, etc.

# Tini allows us to avoid several Docker edge cases, see https://github.com/krallin/tini.
# NOTE: See https://github.com/hexops/dockerfile#is-tini-still-required-in-2020-i-thought-docker-added-it-natively
# RUN apk add --no-cache tini
#RUN mkdir -p /var/lib/pacman
#RUN pacman-key --init
#RUN pacman-key --populate archlinux
#RUN pacman -Syu git make gcc
#RUN cd ~
#RUN mkdir build
#RUN cd build
#RUN git clone https://github.com/leo-arch/clifm
#RUN cd clifm
#RUN make && make install
#RUN clifm -x

#ENTRYPOINT ["/bin/sh", "-c", "clifm"]
# Replace "myapp" above with your binary

# bind-tools is needed for DNS resolution to work in *some* Docker networks, but not all.
# This applies to nslookup, Go binaries, etc. If you want your Docker image to work even
# in more obscure Docker environments, use this.
#RUN apk add --no-cache bind-tools

# Use the non-root user to run our application
USER root

# Default arguments for your app (remove if you have none):
#CMD ["-x", "--cwd-in-title"]
