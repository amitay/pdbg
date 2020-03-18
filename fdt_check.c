#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>

#include <libfdt.h>

static void print_header(void *dtb)
{
	struct fdt_header *h = (struct fdt_header *)dtb;

	printf("magic = %08x (%08x)\n", be32toh(h->magic), h->magic);
	printf("total size = %u (%u)\n", be32toh(h->totalsize), h->totalsize);
	printf("offset to structure = %u (%u)\n", be32toh(h->off_dt_struct), h->off_dt_struct);
	printf("offset to strings = %u (%u)\n", be32toh(h->off_dt_strings), h->off_dt_strings);
	printf("offset to memory reserve map = %u (%u)\n", be32toh(h->off_mem_rsvmap), h->off_mem_rsvmap);
	printf("version = %08x (%08x)\n", be32toh(h->version), h->version);
	printf("last_comp_version = %08x (%08x)\n", be32toh(h->last_comp_version), h->last_comp_version);

	printf("boot cpuid phys = %08x (%08x)\n", be32toh(h->boot_cpuid_phys), h->boot_cpuid_phys);

	printf("size of strings = %u (%u)\n", be32toh(h->size_dt_strings), h->size_dt_strings);

	printf("size of structure = %u (%u)\n", be32toh(h->size_dt_struct), h->size_dt_struct);
}

int main(int argc, const char **argv)
{
	void *dtb;
	struct stat statbuf;
	int fd, ret;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <dtb>\n", argv[0]);
		exit(1);
	}


	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open() failed, errno=%d\n", errno);
		exit(1);
	}

	ret = fstat(fd, &statbuf);
	if (ret == -1) {
		fprintf(stderr, "fstat() failed, errno=%d\n", errno);
		exit(1);
	}

	dtb = mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (dtb == MAP_FAILED) {
		fprintf(stderr, "mmap() failed, errno=%d\n", errno);
		exit(1);
	}

	print_header(dtb);

	munmap(dtb, statbuf.st_size);
	close(fd);
	exit(0);
}
