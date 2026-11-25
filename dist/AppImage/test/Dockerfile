# A minimal desktop-like Linux environment to check that the ImHex AppImage starts.
#
# To test all distributions at once, use run-all.sh. To test a single one:
# Build:  docker build --build-arg BASE_IMAGE=fedora:45 -t imhex-appimage-test dist/AppImage/test
# Run:    docker run --rm --init \
#             --mount type=bind,src=/path/to/ImHex.AppImage,dst=/tmp/ImHex.AppImage,readonly \
#             imhex-appimage-test wayland /tmp/ImHex.AppImage
#

ARG BASE_IMAGE=ubuntu:24.04
FROM ${BASE_IMAGE}

COPY install-packages.sh /tmp/install-packages.sh
RUN bash /tmp/install-packages.sh && rm /tmp/install-packages.sh

# Run ImHex as a regular user, like on a desktop. Xvfb needs this directory to
# exist and be world-writable to create its socket as a non-root user.
RUN useradd --create-home tester && mkdir -p -m 1777 /tmp/.X11-unix

COPY run-test.sh /usr/local/bin/run-test.sh

USER tester
# No GPU in the container: always use Mesa's software renderer.
ENV LIBGL_ALWAYS_SOFTWARE=1

# Give every test its own D-Bus session bus.
ENTRYPOINT ["dbus-run-session", "--", "bash", "/usr/local/bin/run-test.sh"]
