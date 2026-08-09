FROM ghcr.io/netpanzer/netpanzer:build-env-jammy-latest
ARG DEBIAN_FRONTEND=noninteractive
RUN apt update && apt -y upgrade

RUN mkdir /netpanzer
ADD meson* /netpanzer
ADD data /netpanzer/data
ADD po /netpanzer/po
ADD src /netpanzer/src
ADD subprojects /netpanzer/subprojects
ADD tests /netpanzer/tests
ADD --chmod=700 setup-build.sh /netpanzer

# Add docs from the root directory. These will get installed when
# 'meson install' is run
ADD README.md /netpanzer
ADD COPYING.txt /netpanzer
ADD CONTRIBUTING.md /netpanzer
ADD docs/SERVER-HOWTO.md /netpanzer/docs/

# The base image is upstream's build environment, which carries SDL2 dev
# packages and jammy's meson. This branch needs neither: SDL3 comes from the
# wrap in subprojects/, so what has to be here is SDL3's own build
# dependencies, plus a meson new enough to configure it (the wrap requires
# >= 1.6, jammy ships 0.61.2). Installed here rather than in
# Dockerfile-build-env because that image is published by upstream, not built
# from this tree.
RUN apt install -y \
      libasound2-dev \
      libdbus-1-dev \
      libegl-dev \
      libfreetype-dev \
      liblua5.4-dev \
      libpulse-dev \
      libudev-dev \
      libwayland-dev \
      libxkbcommon-dev \
      python3-pip \
      wayland-protocols && \
  python3 -m pip install --upgrade pip && \
  pip install meson ninja

RUN \
  cd /netpanzer && \
  ./setup-build.sh _build \
    -Dprefix=/usr \
    -Dbuildtype=release -Dstrip=true && \
  cd _build && \
  ninja && \
  meson install --skip-subprojects

FROM ubuntu:jammy
LABEL org.opencontainers.image.source=https://github.com/netpanzer/netpanzer
COPY --from=0 /usr/bin/netpanzer /usr/bin
COPY --from=0 /usr/share/netpanzer /usr/share/netpanzer
COPY --from=0 /netpanzer/_build/po/* /usr/share/locale

ARG DEBIAN_FRONTEND=noninteractive
RUN apt update &&   \
    apt -y upgrade && \
# Runtime libraries only. SDL3 is linked into the binary from the wrap, so
# no SDL package is needed here -- but the libraries SDL3 itself links are,
# or the server will not start. libasan/libubsan are gone with the sanitizers,
# which are opt-in now rather than on by default.
    apt install -y  \
        libasound2 \
        libdbus-1-3 \
        libegl1 \
        libfreetype6 \
        libharfbuzz0b \
        liblua5.4-0 \
        libphysfs1 \
        libpulse0 \
        libudev1 \
        libwayland-client0 \
        libwayland-cursor0 \
        libwayland-egl1 \
        libx11-6 \
        libxkbcommon0 \
        sudo

RUN useradd -m npserver && passwd -d npserver && \
  echo "npserver ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

USER npserver
WORKDIR /home/npserver

CMD ["/bin/bash","-l"]
