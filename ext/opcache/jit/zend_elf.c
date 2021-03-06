/*
   +----------------------------------------------------------------------+
   | Zend JIT                                                             |
   +----------------------------------------------------------------------+
   | Copyright (c) 1998-2016 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Dmitry Stogov <dmitry@zend.com>                             |
   |          Xinchen Hui <xinchen.h@zend.com>                            |
   +----------------------------------------------------------------------+
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "zend_API.h"
#include "zend_elf.h"

static void* zend_elf_read_sect(int fd, zend_elf_sectheader *sect)
{
	void *s = emalloc(sect->size);

	if (lseek(fd, sect->ofs, SEEK_SET) < 0) {
		efree(s);
		return NULL;
	}
	if (read(fd, s, sect->size) != (ssize_t)sect->size) {
		efree(s);
		return NULL;
	}

	return s;
}

void zend_elf_load_symbols(void)
{
	zend_elf_header hdr;
	zend_elf_sectheader sect;
	int i;
	int fd = open("/proc/self/exe", O_RDONLY);

	if (fd >= 0) {
		if (read(fd, &hdr, sizeof(hdr)) == sizeof(hdr)
		 && hdr.emagic[0] == '\177'
		 && hdr.emagic[1] == 'E'
		 && hdr.emagic[2] == 'L'
		 && hdr.emagic[3] == 'F'
		 && lseek(fd, hdr.shofs, SEEK_SET) >= 0) {
			for (i = 0; i < hdr.shnum; i++) {
				if (read(fd, &sect, sizeof(sect)) == sizeof(sect)
				 && sect.type == ELFSECT_TYPE_SYMTAB) {
					uint32_t n, count = sect.size / sizeof(zend_elf_symbol);
					zend_elf_symbol *syms = zend_elf_read_sect(fd, &sect);
					char *str_tbl;

					if (syms) {
						if (lseek(fd, hdr.shofs + sect.link * sizeof(sect), SEEK_SET) >= 0
						 && read(fd, &sect, sizeof(sect)) == sizeof(sect)
						 && (str_tbl = (char*)zend_elf_read_sect(fd, &sect)) != NULL) {
							for (n = 0; n < count; n++) {
								if (syms[n].name
								 && (syms[n].info & ELFSYM_TYPE_FUNC)
								 &&	!(syms[n].info & ELFSYM_BIND_GLOBAL)) {
									zend_jit_disasm_add_symbol(str_tbl + syms[n].name, syms[n].value, syms[n].size);
								}
							}
							efree(str_tbl);
						}
						efree(syms);
					}
					if (lseek(fd, hdr.shofs + (i + 1) * sizeof(sect), SEEK_SET) < 0) {
						break;
					}
				}
			}
		}
		close(fd);
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
