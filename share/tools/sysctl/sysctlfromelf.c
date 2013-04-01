/*-
 * Copyright (c) 2012-2013 Dag-Erling Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/stat.h>

#define _KERNEL
#include <sys/sysctl.h>
#undef _KERNEL

#include <assert.h>
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "elfexplorer.h"
#include "util.h"

static void process_oid(const struct sysctl_oid *);

/*
 * Process an OID.
 */
static void
process_oid(const struct sysctl_oid *oid)
{
	const struct sysctl_oid_list *children;
	const struct sysctl_oid *child;

	printf("<sysctl:node name='%s'>\n", oid->oid_name);
	if (oid->oid_descr)
		printf("<sysctl:short><![CDATA[%s]]></sysctl:short>\n",
		    oid->oid_descr);
//	printf("<sysctl:kind>%08x</sysctl:kind>\n", oid->oid_kind);
	if ((oid->oid_kind & CTLTYPE) == CTLTYPE_NODE) {
		children = (const struct sysctl_oid_list *)oid->oid_arg1;
		SLIST_FOREACH(child, children, oid_link)
			process_oid(child);
	}
	printf("</sysctl:node>\n");
}

/*
 * Process an individual file.
 */
static int
process_file(const char *path)
{
	elfx_file *ef;
	elfx_section *sect;
	unsigned int nsysctls;
	struct sysctl_oid_list *sysctl_root;
	struct sysctl_oid *oid, **oids;
	uintptr_t rootaddr;

	if ((ef = elfx_open(path)) == NULL)
		return (-1);
	/* find the root list */
	rootaddr = elfx_get_symbol(ef, "sysctl__children");
	sect = elfx_get_section_by_addr(ef, rootaddr, NULL);
	assert(sect != NULL);
	sysctl_root = elfx_get_data(sect, rootaddr);
	/* find the sysctl linker set */
	if ((sect = elfx_get_section_by_name(ef, "set_sysctl_set")) == NULL) {
		verbose("no sysctl linker set");
		elfx_close(ef);
		return (0);
	}
	assert(sect->size % sizeof(uintptr_t) == 0);
	nsysctls = sect->size / sizeof(uintptr_t);
	oids = (struct sysctl_oid **)sect->ptr;
	verbose("%zd sysctls found in section %d", nsysctls, sect->index);
	/* retrieve OIDs and fix up various pointers */
	for (unsigned int i = 0; i < nsysctls; ++i) {
		sect = elfx_get_section_by_addr(ef, (uintptr_t)oids[i], sect);
		assert(sect != NULL);
		oids[i] = oid = elfx_get_data(sect, oids[i]);
		/* name */
		sect = elfx_get_section_by_addr(ef,
		    (uintptr_t)oid->oid_name, sect);
		assert(sect != NULL);
		oid->oid_name = elfx_get_data(sect, oid->oid_name);
		/* descriptions */
		if (oid->oid_descr != NULL) {
			sect = elfx_get_section_by_addr(ef,
			    (uintptr_t)oid->oid_descr, sect);
			assert(sect != NULL);
			oid->oid_descr = elfx_get_data(sect, oid->oid_descr);
		}
		/* siblings */
		if (oid->oid_link.sle_next != NULL) {
			sect = elfx_get_section_by_addr(ef,
			    (uintptr_t)oid->oid_link.sle_next, sect);
			assert(sect != NULL);
			oid->oid_link.sle_next = elfx_get_data(sect,
			    oid->oid_link.sle_next);
		}
		/* children */
		if ((oid->oid_kind & CTLTYPE) == CTLTYPE_NODE &&
		    oid->oid_arg1 != 0) {
			sect = elfx_get_section_by_addr(ef,
			    (uintptr_t)oid->oid_arg1, sect);
			assert(sect != NULL);
			oid->oid_arg1 = elfx_get_data(sect,
			    (uintptr_t)oid->oid_arg1);
		}
		/* parent */
		sect = elfx_get_section_by_addr(ef,
		    (uintptr_t)oid->oid_parent, sect);
		assert(sect != NULL);
		oid->oid_parent = elfx_get_data(sect, oid->oid_parent);
		SLIST_INSERT_HEAD(oid->oid_parent, oid, oid_link);
	}
	/* list all OIDs! */
	printf("<?xml version='1.0' encoding='utf-8'?>\n"
	    "<sysctl:tree xmlns:sysctl='http://www.FreeBSD.org/XML/sysctl'>\n");
	SLIST_FOREACH(oid, sysctl_root, oid_link) {
		process_oid(oid);
	}
	printf("</sysctl:tree>\n");
	elfx_close(ef);
	return (0);
}

/*
 * Traverse a directory and process any file it named "kernel" or "*.ko".
 */
static int
process_directory(const char *path)
{
	DIR *dir;
	struct dirent ent, *res;
	char pathbuf[PATH_MAX];
	int ret;

	verbose("%s(%s)", __func__, path);
	if ((dir = opendir(path)) == NULL) {
		warn("%s", path);
		return (-1);
	}
	ret = 0;
	for (;;) {
		if (readdir_r(dir, &ent, &res) != 0) {
			warn("%s", path);
			closedir(dir);
			return (-1);
		}
		if (res == NULL)
			break;
		assert(res == &ent);
		if (snprintf(pathbuf, sizeof(pathbuf), "%s/%s", path, ent.d_name) >=
		    (int)sizeof(pathbuf)) {
			errno = ENAMETOOLONG;
			warn("%s/%s", path, ent.d_name);
			continue;
		}
		if (strcmp(ent.d_name, "kernel") == 0 ||
		    (ent.d_namlen > 4 &&
		    strcmp(ent.d_name + ent.d_namlen - 3, ".ko") == 0))
			if (process_file(pathbuf) != 0)
				ret = -1;
	}
	closedir(dir);
	return (ret);
}

/*
 * Check whether a path is a file or a directory and pass it on to the
 * appropriate processing function.
 */
static int
process_path(const char *path)
{
	struct stat sb;

	verbose("%s(%s)", __func__, path);
	if (stat(path, &sb) != 0) {
		warn("%s", path);
		return (-1);
	}
	switch (sb.st_mode & S_IFMT) {
	case S_IFREG:
		return (process_file(path));
	case S_IFDIR:
		return (process_directory(path));
	default:
		warnx("%s is neither a regular file nor a directory", path);
		return (-1);
	}
}

/*
 * Print usage message and exit.
 */
static int
usage(void)
{

	fprintf(stderr, "usage: sysctlfromelf [-v] [path ...]\n");
	exit(1);
}

/*
 * Main loop
 */
int
main(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "v")) != -1)
		switch (opt) {
		case 'v':
			++opt_v;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (elf_version(EV_CURRENT) == EV_NONE)
		errx(1, "incorrect ELF library version");
	verbose("ELF library version %d", elf_version(EV_NONE));

	if (argc == 0)
		process_path("/boot/kernel");
	else
		while (argc-- > 0)
			process_path(*argv++);

	exit(0);
}
