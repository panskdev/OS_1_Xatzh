FROM ubuntu:20.04

RUN apt-get update && apt-get install -y build-essential
WORKDIR /chat_app
COPY . /chat_app

RUN make compile || true
RUN echo "Usage: ./chat <chat_id>"
