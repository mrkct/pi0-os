.PHONY: all

APPS := init shell wm cowsay crash kilo
DIR := /tmp/myos-disk-image-temp-storage

.PHONY: all clean build-apps build-% clean-apps clean-% clean-disk-image

all: disk-image
clean: clean-apps clean-disk-image

BUILD_TARGETS = $(addprefix build-,$(APPS))
CLEAN_TARGETS = $(addprefix clean-,$(APPS))
build-apps: $(BUILD_TARGETS)
clean-apps: $(CLEAN_TARGETS)

build-%:
	@echo "\033[32mBuilding '$*'\033[0m"
	$(MAKE) -C $*

clean-%:
	@echo "\033[31mCleaning '$*'\033[0m"
	$(MAKE) -C $* clean

disk-image: build-apps
	@echo "\033[32mBuilding disk image...\033[0m"
	rm -rf $(DIR)
	mkdir -p $(DIR)
	cp -r base/* $(DIR)

	@for name in $(APPS); do \
    	echo "Installing '$$name'..."; \
    	MOUNT_POINT=$(DIR) $(MAKE) -C $$name install; \
	done

	rm -f _disk_image.img
	dd if=/dev/zero of=_disk_image.img bs=1M count=16
	mkfs.fat -F 32 _disk_image.img
	mcopy -i _disk_image.img $(DIR)/* ::/
	qemu-img convert -p -O qcow2 _disk_image.img _disk_image.qcow2
	rm -rf $(DIR)

clean-disk-image:
	rm -rf $(DIR)
	rm -f _disk_image.img
	rm -f _disk_image.qcow2
