FROM thyrlian/android-sdk:7.0
# Install Python
# cf. https://pythonspeed.com/articles/official-python-docker-image/
ENV PATH /usr/local/bin:$PATH
ENV LANG C.UTF-8

# Install Bazel & CPP build chain
RUN apt update && apt install -y curl gnupg
RUN curl -fsSL https://bazel.build/bazel-release.pub.gpg | gpg --dearmor > bazel.gpg
RUN mv bazel.gpg /etc/apt/trusted.gpg.d/
RUN echo "deb [arch=amd64] https://storage.googleapis.com/bazel-apt stable jdk1.8" | tee /etc/apt/sources.list.d/bazel.list
RUN apt update && apt install -y bazel libopenblas-dev build-essential
RUN apt update && apt install -y cmake git


# Install Android dependencies (NDK)
WORKDIR /opt/android-sdk/cmdline-tools/tools/bin
RUN yes | sdkmanager --licenses
RUN sdkmanager --install 'ndk;21.3.6528147'
ENV ANDROID_HOME=/opt/android-sdk
ENV ANDROID_NDK=/opt/android-sdk/ndk/21.3.6528147

# Python setup
RUN apt install -y python3-pip
RUN apt install -y python-is-python3

# Install PyTorch
RUN mkdir -p /usr/local/include/pocketfft
COPY docker/pocketfft_hdronly.h /usr/local/include/pocketfft 
WORKDIR /
RUN mkdir pytorch # CMake doesn't like running in the root directory cf. https://gitlab.kitware.com/cmake/cmake/-/issues/16603
WORKDIR pytorch
RUN pip install pyyaml
RUN pip install typing_extensions

