FROM ubuntu-debootstrap:14.04

COPY ./ /server
WORKDIR /server
RUN apt-get update
RUN apt-get install build-essential -y
RUN make
