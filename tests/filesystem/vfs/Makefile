SOURCES := \
	$(PROJ_ROOT)/kernel/vfs/fs.cpp \
	$(PROJ_ROOT)/kernel/vfs/fat32/fat32.cpp \
	$(PROJ_ROOT)/kernel/drivers/device.cpp \
	$(PROJ_ROOT)/kernel/drivers/block/ramdisk.cpp \

samplefs.bin:
	dd if=/dev/zero of=samplefs.bin bs=1M count=16
	mkfs.vfat -F 32 samplefs.bin
	mkdir -p /tmp/samplefs_mount
	sudo mount -o loop samplefs.bin /tmp/samplefs_mount
	sudo cp -R ./samplefs/* /tmp/samplefs_mount/
	sudo umount /tmp/samplefs_mount
	rmdir /tmp/samplefs_mount

test: samplefs.bin
	$(CXX) $(CXXFLAGS) $(SOURCES) test_vfs.cpp -o test_vfs && ./test_vfs
