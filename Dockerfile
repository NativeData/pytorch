FROM python:slim-buster
RUN apt update && apt install -y curl gnupg
RUN curl -fsSL https://bazel.build/bazel-release.pub.gpg | gpg --dearmor > bazel.gpg
RUN mv bazel.gpg /etc/apt/trusted.gpg.d/
RUN echo "deb [arch=amd64] https://storage.googleapis.com/bazel-apt stable jdk1.8" | tee /etc/apt/sources.list.d/bazel.list
RUN apt update && apt install -y bazel libopenblas-dev build-essential
RUN apt update && apt install -y cmake git
RUN mkdir -p /usr/local/include/pocketfft
COPY docker/pocketfft_hdronly.h /usr/local/include/pocketfft 
RUN mkdir pytorch # CMake doesn't like running in the root directory cf. https://gitlab.kitware.com/cmake/cmake/-/issues/16603
WORKDIR pytorch
RUN pip install pyyaml
RUN pip install typing_extensions

