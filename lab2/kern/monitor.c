// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line
#define COLOR_WHT 7;
#define COLOR_BLK 0;
#define COLOR_BLU 1;
#define COLOR_GRN 2;
#define COLOR_RED 4;
#define COLOR_GRY 8;
#define COLOR_YLW 15;
#define COLOR_ORG 12;
#define COLOR_PUR 6;
#define COLOR_CYN 11;

struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "setcolor", "set the color for text", mon_setcolor },
	{ "backtrace", "Display information about ebp & eip", mon_backtrace },
	{ "showmappings", "DisPlay the physical page mappings and corresponding permission bits that apply to the pages\n               showmappings a b, display the information from a to b", mon_showmapping },
	{ "setp", "setp Permission, Clear the address's permission by default\n       setp va PTE_U PTE_W PTE_W", mon_changePermission },
	{ "dump", "dump the virtual or physical address of the memory", mon_dump },
	{ "PT", "show the page table of given address", mon_showPT }
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int mon_setcolor(int argc, char **argv, struct Trapframe *tf) {
	//argv
	uint16_t ch_color1, ch_color;
	if(strcmp(argv[2],"blk")==0)
			ch_color1=COLOR_BLK
	else if(strcmp(argv[2],"wht")==0)
			ch_color1=COLOR_WHT
	else if(strcmp(argv[2],"blu")==0)
			ch_color1=COLOR_BLU
	else if(strcmp(argv[2],"grn")==0)
			ch_color1=COLOR_GRN
	else if(strcmp(argv[2],"red")==0)
			ch_color1=COLOR_RED
	else if(strcmp(argv[2],"gry")==0)
			ch_color1=COLOR_GRY
	else if(strcmp(argv[2],"ylw")==0)
			ch_color1=COLOR_YLW
	else if(strcmp(argv[2],"org")==0)
			ch_color1=COLOR_ORG
	else if(strcmp(argv[2],"pur")==0)
			ch_color1=COLOR_PUR
	else if(strcmp(argv[2],"cyn")==0)
			ch_color1=COLOR_CYN
	else ch_color1=COLOR_WHT;
	if(strcmp(argv[1],"blk")==0)
			ch_color=COLOR_BLK
	else if(strcmp(argv[1],"wht")==0)
			ch_color=COLOR_WHT
	else if(strcmp(argv[1],"blu")==0)
			ch_color=COLOR_BLU
	else if(strcmp(argv[1],"grn")==0)
			ch_color=COLOR_GRN
	else if(strcmp(argv[1],"red")==0)
			ch_color=COLOR_RED
	else if(strcmp(argv[1],"gry")==0)
			ch_color=COLOR_GRY
	else if(strcmp(argv[1],"ylw")==0)
			ch_color=COLOR_YLW
	else if(strcmp(argv[1],"org")==0)
			ch_color=COLOR_ORG
	else if(strcmp(argv[1],"pur")==0)
			ch_color=COLOR_PUR
	else if(strcmp(argv[1],"cyn")==0)
			ch_color=COLOR_CYN
	else ch_color=COLOR_WHT;
	set_attribute_color((uint64_t) ch_color, (uint64_t) ch_color1);
	cprintf("console back-color :  %d \n        fore-color :  %d\n", ch_color, ch_color1);	
	return 0;
}

void printPermission(pte_t now) {
	cprintf("PTE_U : %d ", ((now & PTE_U) != 0));
	cprintf("PTE_W : %d ", ((now & PTE_W) != 0));
	cprintf("PTE_P : %d ", ((now & PTE_P) != 0));
}

uint32_t xtoi(char* origin, bool* check) {
	uint32_t i = 0, temp = 0, len = strlen(origin);
	*check = true;
	if ((origin[0] != '0') || (origin[1] != 'x' && origin[1] != 'X')) 
	{
		*check = false;
		return -1;
	}
	for (i = 2; i < len; i++) {
		temp *= 16;
		if (origin[i] >= '0' && origin[i] <= '9')
			temp += origin[i] - '0';
		else if (origin[i] >= 'a' && origin[i] <= 'f')
			temp += origin[i] - 'a' + 10;
		else if (origin[i] >= 'A' && origin[i] <= 'F')
			temp += origin[i] - 'A' + 10;
		else {
			*check = false;
			return -1;
		}
	}
	return temp;
}

bool pxtoi(uint32_t *va, char *origin) {
	bool check = true;
	*va = xtoi(origin, &check);
	if (!check) {
		cprintf("Address typing error\n");
		return false;
	}
	return true;
}

int mon_changePermission(int argc, char **argv, struct Trapframe *tf) 
{
	if (argc < 2) {
		cprintf("invalid number of parameters\n");
		return 0;
	}
	uintptr_t va;
	if (!pxtoi(&va,argv[1]))	return 0;
	
	pte_t* mapper = pgdir_walk(kern_pgdir, (void*) va, 1);
	if (!mapper) 
		panic("error, out of memory");
	physaddr_t pa = PTE_ADDR(*mapper);
	int perm = 0;
	//PTE_U PET_W PTE_P
	if (argc != 2) {
		if (argc != 5) {
			cprintf("invalid number of parameters\n");
			return 0;
		}
		if (argv[2][0] == '1') perm |= PTE_U;
		if (argv[3][0] == '1') perm |= PTE_W;
		if (argv[4][0] == '1') perm |= PTE_P;
	}
//	boot_map_region(kern_pgdir, va, PGSIZE, pa, perm);	
	cprintf("before change "); printPermission(*mapper); cprintf("\n");
	
	*mapper = PTE_ADDR(*mapper) | perm;
	cprintf("after change ");  printPermission(*mapper); cprintf("\n");
	return 0;
}


int mon_showPT(int argc, char **argv, struct Trapframe *tf) {
	extern int gdtdesc;
	cprintf("%x", *((uint32_t*)KADDR(GD_KT)));
/*	uintptr_t va;
	if (!pxtoi(&va, argv[1])) return 0;
	
	pte_t *mapper = pgdir_walk(kern_pgdir, (void*) ((va >> PDXSHIFT) << PDXSHIFT), 1);
	cprintf("Page Table Entry Address : 0x%08x\n", mapper); */
	return 0;
}
#define POINT_SIZE 4
int mon_dump(int argc, char **argv, struct Trapframe *tf) {
	uint32_t begin, end;
	if (argc < 3) {
		cprintf("invalid command\n");
		return 0;
	}
	if (!pxtoi(&begin, argv[2])) return 0;
	if (!pxtoi(&end, argv[3])) return 0;
	if (argv[1][0] == 'p') {
		if (PGNUM(end) >= npages || PGNUM(end) >= npages){
			cprintf("out of memory\n");
			return 0;	
		}
		for (;begin <= end; begin += POINT_SIZE)
			cprintf("pa 0x%08x : 0x%08x\n", begin, *((uint32_t*)KADDR(begin)));
	} else if (argv[1][0] == 'v') {
		for (;begin <= end; begin+=POINT_SIZE) {
			cprintf("Va 0x%08x : 0x%08x\n", begin, *((uint32_t*)begin));
		}
	} else cprintf("invalid command\n");
	return 0;

}
int mon_showmapping(int argc, char **argv, struct Trapframe *tf) 
{
	uintptr_t begin, end;
	if (!pxtoi(&begin, argv[1])) return 0;
//	cprintf("%d", !pxtoi(&begin, argv[1]));
	if (!pxtoi(&end, argv[2])) return 0;
	begin = ROUNDUP(begin, PGSIZE); 
	end   = ROUNDUP(end, PGSIZE);
	for (;begin <= end; begin += PGSIZE) {
		pte_t *mapper = pgdir_walk(kern_pgdir, (void*) begin, 1);
		cprintf("VA 0x%08x : ", begin);
		if (mapper != NULL) {
			if (*mapper & PTE_P) {
				cprintf("mapping 0x%08x ", PTE_ADDR(*mapper));//, PADDR((void*)begin));
				printPermission((pte_t)*mapper);
				cprintf("\n");
			} else {
				cprintf("page not mapping\n");
			}
		} else {
			panic("error, out of memory");
		}
	}
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
// Your code here.
//	cprintf("%08x", read_ebp());
	uint32_t *eip, *ebp;
	ebp = (uint32_t*) read_ebp();
	eip = (uint32_t*) ebp[1];
	cprintf("Stackbacktrace:\n");
	while (ebp!=0) {
		cprintf("ebp %08x eip %08x args %08x %08x %08x %08x %08x\n",ebp, eip, ebp[2] ,ebp[3], ebp[4], ebp[5] ,ebp[6]);
		struct Eipdebuginfo temp_debuginfo;
		debuginfo_eip((uintptr_t) eip, &temp_debuginfo);		
		cprintf("       %s:%d: ", temp_debuginfo.eip_file, temp_debuginfo.eip_line);
		uint32_t i = 0;// = temp_debuginfo.eip_fn_namelen;
		while  (i < temp_debuginfo.eip_fn_namelen){
			cprintf("%c", temp_debuginfo.eip_fn_name[i]);
			i++;	
		}
		int p = (int)eip;
		int q = (int)temp_debuginfo.eip_fn_addr;
		cprintf("+%x\n", p - q);
		ebp=(uint32_t*)ebp[0];
		eip=(uint32_t*)ebp[1]; 
	}
	
//	cprintf("%d", read_esp());
	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");
//	int x = 1, y = 3, z = 4;
  //	cprintf("x %d, y %x, z %d\n", x, y, z);
//	unsigned int i = 0x00646c72;
//	cprintf("H%x Wo%s", 57616, &i);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
