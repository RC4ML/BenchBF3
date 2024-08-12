FROM nvcr.io/nvidia/doca/doca:2.7.0-devel

# RUN sed -i 's/ports.ubuntu.com/mirrors.zju.edu.cn/g' /etc/apt/sources.list

# Install SSH server
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends openssh-server unzip libgflags-dev libz-dev liblz4-dev && \
    rm -rf /var/lib/apt/lists/*


RUN echo 'root:cxz123' | chpasswd

RUN wget https://github.com/protocolbuffers/protobuf/releases/download/v3.20.3/protobuf-cpp-3.20.3.zip && unzip protobuf-cpp-3.20.3.zip && cd protobuf-3.20.3 && ./configure && make -j&& make install && ldconfig

# Configure SSH server
RUN mkdir /var/run/sshd && \
    sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config && \
    sed -i 's/#PasswordAuthentication yes/PasswordAuthentication yes/' /etc/ssh/sshd_config


EXPOSE 22
CMD ["/usr/sbin/sshd", "-D"]