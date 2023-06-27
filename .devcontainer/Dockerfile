FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-20.04

ARG REINSTALL_CMAKE_VERSION_FROM_SOURCE="none"

# Optionally install the cmake for vcpkg
COPY ./reinstall-cmake.sh /tmp/

RUN if [ "${REINSTALL_CMAKE_VERSION_FROM_SOURCE}" != "none" ]; then \
        chmod +x /tmp/reinstall-cmake.sh && /tmp/reinstall-cmake.sh ${REINSTALL_CMAKE_VERSION_FROM_SOURCE}; \
    fi \
    && rm -f /tmp/reinstall-cmake.sh

# [Optional] Uncomment this section to install additional vcpkg ports.
# RUN su vscode -c "${VCPKG_ROOT}/vcpkg install <your-port-name-here>"

# [Optional] Uncomment this section to install additional packages.
#RUN apt-get update && sudo apt-get upgrade \
#    && export DEBIAN_FRONTEND=noninteractive \
#    && apt-get -y install --no-install-recommends \ 
#    <package list>
# install for code checkout: git
# install for gcc compiler & debugger : build-essential 
# install for ssl: openssl libssl-dev libssl1.0
# install for pdftops: poppler-utils
# install for Mesa OSS Radeon, Intel, & Nvidia graphics driver: libgl1-mesa-dev
# install for Qt: qt5-default qt5-qmake qtbase5-dev-tools libqt5x11extras5 
# install for QtMultimedia: gstreamer1.0-plugins-base
# install for Qt xcb: libxkbcommon
# install for appimage: libfuse2
RUN apt-get update && sudo apt-get upgrade \
    && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \ 
    git build-essential \
    openssl libssl-dev libssl1.0 poppler-utils libgl1-mesa-dev \
    qt5-default qt5-qmake qtbase5-dev-tools libqt5x11extras5  \
    gstreamer1.0-plugins-base libxkbcommon-x11-0 \
    libfuse2 

# build appimage
#RUN sudo wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage -O /usr/local/bin/linuxdeploy-x86_64.AppImage
#RUN sudo chmod +x /usr/local/bin/linuxdeploy-x86_64.AppImage
#RUN sudo wget -q https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage -O /usr/local/bin/linuxdeploy-plugin-qt-x86_64.AppImage
#RUN sudo chmod +x /usr/local/bin/linuxdeploy-plugin-qt-x86_64.AppImage


