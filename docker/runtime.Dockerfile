# This image only contains the solver runtime dependencies
# This image is public to be able to run on cloud providers such as Vast.ai
# We upload the required binary to the container at runtime

FROM ubuntu:22.04

WORKDIR /app

RUN apt update -y && apt install -y screen curl libtbb-dev
RUN touch ~/.no_auto_tmux

COPY problems /app/problems
