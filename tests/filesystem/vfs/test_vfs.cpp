#include "libtest.h"
#include <kernel/drivers/block/ramdisk.h>
#include <kernel/vfs/fat32/fat32.h>
#include <kernel/vfs/vfs.cpp>


static RamDisk *s_ramdisk;
static Filesystem *s_fs;


TEST(open_file)
{
	FileCustody *custody;
	ASSERT_EQUAL_INT(0, vfs_open("/dir1/dir2/hello", O_RDONLY, &custody));

	const char *expected = "Hello, world!";
	uint8_t contents[100];
	ASSERT_EQUAL_INT<ssize_t>(strlen(expected), vfs_read(custody, contents, sizeof(contents)));

	ASSERT_TRUE(0 == memcmp(expected, contents, strlen(expected)));

	ASSERT_EQUAL_INT<int>(0, vfs_read(custody, contents, sizeof(contents)));

	vfs_close(custody);
}

TEST(open_file_twice)
{
	FileCustody *custody1, *custody2;
	ASSERT_EQUAL_INT(0, vfs_open("/dir1/dir2/hello", O_RDONLY, &custody1));
	ASSERT_EQUAL_INT(0, vfs_open("/dir1/dir2/hello", O_RDONLY, &custody2));

	uint8_t buf1[100], buf2[100];
	ssize_t read1 = vfs_read(custody1, buf1, sizeof(buf1));
	ASSERT_TRUE(read1 > 0);
	ssize_t read2 = vfs_read(custody2, buf2, sizeof(buf2));
	ASSERT_TRUE(read2 > 0);

	ASSERT_EQUAL_INT(read1, read2);
	ASSERT_TRUE(0 == memcmp(buf1, buf2, read1));

	vfs_close(custody1);
	vfs_close(custody2);
}

TEST(large_file)
{
	FileCustody *custody;
	ASSERT_EQUAL_INT(0, vfs_open("/dir3/64k", O_RDONLY, &custody));

#define BUF_SIZE 64*1024
	uint8_t *buf = (uint8_t*) malloc(BUF_SIZE);
	ASSERT_EQUAL_INT<ssize_t>(BUF_SIZE, vfs_read(custody, buf, BUF_SIZE));

	// Check the data is correct
	for (int i = 0; i < BUF_SIZE / 512; i++) {
		uint8_t testbuf[512];
		memset(testbuf, i, sizeof(testbuf));

		ASSERT_TRUE(0 == memcmp(buf + i * 512, testbuf, sizeof(testbuf)));
	}

	free(buf);
	vfs_close(custody);
}

TEST(file_not_found)
{
	FileCustody *custody;
	ASSERT_EQUAL_INT<int>(-ENOENT, vfs_open("/dir1/dir2/doesnotexist", O_RDONLY, &custody));
}

static void load_resources()
{
	uint8_t *buf;
	size_t size;
	load_file("samplefs.bin", &buf, &size);
	s_ramdisk = new RamDisk(buf, size, false);
	if (0 != fat32_try_create(*s_ramdisk, &s_fs)) {
		fprintf(stderr, "failed to mount filesystem for tests\n");
		exit(1);
	}

	vfs_mount("/", *s_fs);
}

static void free_resources()
{
}

int main(int argc, char **argv) {
	load_resources();
	run_tests(argc, argv);
	free_resources();
	return 0;
}
