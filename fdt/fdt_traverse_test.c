/* Copyright 2019 IBM Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "fdt_traverse.h"

static void *print_node(const char *name, void *parent, void *priv)
{
	printf("Node: %s\n", name);
	return (void *)1;
}

static int print_property(void *node, const char *name, void *value, int valuelen, void *priv)
{
	printf("  %s: length=%u\n", name, valuelen);
	return 0;
}

int main(int argc, const char **argv)
{
	void *fdt;
	struct stat statbuf;
	int fd, ret;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <dtb>\n", argv[0]);
		exit(1);
	}

	fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
		perror("open");
		exit(1);
	}

	ret = fstat(fd, &statbuf);
	if (ret == -1) {
		perror("fstat");
		exit(1);
	}

	fdt = mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (fdt == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	fdt_traverse_read(fdt, (void *)1, print_node, print_property, NULL);

	munmap(fdt, statbuf.st_size);
	close(fd);

	return 0;
}
