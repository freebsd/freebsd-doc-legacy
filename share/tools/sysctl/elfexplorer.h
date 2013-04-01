/*-
 * Copyright (c) 2013 Dag-Erling Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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

#ifndef ELFEXPLORER_H_INCLUDED
#define ELFEXPLORER_H_INCLUDED

#include <libelf.h>
#include <gelf.h>

typedef struct elfx_file {
	char			 path[PATH_MAX];
	int			 fd;
	size_t			 size;
	void			*map;
	Elf			*elf;
	GElf_Ehdr		 hdr;
	unsigned int		 nsections;
	struct elfx_section	*sections;
	struct elfx_section	*sections_by_name;
	struct elfx_section	*last_section_by_name;
	struct elfx_section	*sections_by_addr;
	struct elfx_section	*last_section_by_addr;
	unsigned int		 nsymbols;
	struct elfx_symbol	*symbols;
	struct elfx_symbol	*last_symbol_by_name;
} elfx_file;

typedef struct elfx_section {
	struct elfx_file	*file;
	int			 index;
	Elf_Scn			*scn;
	GElf_Shdr		 hdr;
	char			*name;
	Elf_Data		*data;
	uintptr_t		 baddr;
	uintptr_t		 eaddr;
	size_t			 size;
	void			*ptr;
} elfx_section;

typedef struct elfx_symbol {
	struct elfx_file	*file;
	char			*name;
	uintptr_t		 addr;
	size_t			 size;
} elfx_symbol;

elfx_file *elfx_open(const char *);
void elfx_close(elfx_file *);

elfx_section *elfx_get_section_by_name(elfx_file *, const char *);
elfx_section *elfx_get_section_by_addr(elfx_file *, uintptr_t);
void *elfx_get_data(elfx_file *, uintptr_t);
elfx_symbol *elfx_get_symbol_by_name(elfx_file *, const char *);


#endif
