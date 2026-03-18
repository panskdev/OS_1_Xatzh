FROM ubuntu:20.04

RUN apt-get update && apt-get install -y build-essential git
WORKDIR /chat_app
RUN git clone https://github.com/panskdev/OS_1_Xatzh.git && cd OS_1_Xatzh && mkdir build && make compile
RUN echo "Usage: ./chat <chat_id>"
