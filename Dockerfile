FROM alpine:latest

RUN apk update && \
    apk add --no-cache sudo make dosfstools gcc-arm-none-eabi qemu-system-arm qemu-img
