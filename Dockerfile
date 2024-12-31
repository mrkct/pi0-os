FROM alpine:latest

RUN apk update && \
    apk add --no-cache sudo make dosfstools mtools gcc-arm-none-eabi g++-arm-none-eabi qemu-system-arm qemu-img

WORKDIR /pi0-os
ENTRYPOINT ["scripts/entrypoint.sh"]
