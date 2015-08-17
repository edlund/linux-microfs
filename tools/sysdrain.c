/* microfs - Minimally Improved Compressed Read Only File System
 * Copyright (C) 2014 Erik Edlund <erik.edlund@32767.se>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "../microfs.h"
#include "../hostprogs.h"

#include <sys/sysinfo.h>

#define SYSDRAIN_MINRANDSHIFT 10
#define SYSDRAIN_MAXRANDSHIFT 17

#define SYSDRAIN_OPTIONS "hves:m:"

struct sysdrain {
	/* Number of allocated pointer slots. */
	__u64 d_slots;
	/* Pointers to allocated memory. */
	void** d_ptrslots;
	/* Size of pointers in %d_ptrslots. */
	__u64* d_szslots;
	/* The last occupied index of %d_ptrslots. */
	__u64 d_end;
	/* Total size of allocated memory. */
	__u64 d_memsz;
};

struct sysdrainoptions {
	/* Random seed. */
	unsigned int so_seed;
	/* User requested drain percentage. */
	__u64 so_targetpercent;
	/* Target size based on given %so_targetpercent. */
	__u64 so_targetsz;
	/* Approx. max allocated memory. */
	__u64 so_ceilsz;
	/* Approx. min allocated memory. */
	__u64 so_floorsz;
};

typedef void (*drain_task)(struct sysdrain*);

static void drain_task_memset0(struct sysdrain* drain)
{
	message(VERBOSITY_1, "drain_task_memset0");
	__u64 slot = rand_nonuniform_range(0, drain->d_end);
	__u64 slotsz = drain->d_szslots[slot];
	memset(drain->d_ptrslots[slot], 0, slotsz);
}

static void drain_task_read(struct sysdrain* drain, const char* path)
{
	int rdfd = open(path, O_RDONLY, 0);
	if (rdfd < 0) {
		error("failed to open path \"%s\": %s", path, strerror(errno));
	}
	__u64 slot = rand_nonuniform_range(0, drain->d_end);
	__u64 slotsz = drain->d_szslots[slot];
	if (read(rdfd, drain->d_ptrslots[slot], slotsz) < 0) {
		error("failed to read \"%s\": %s", path, strerror(errno));
	}
	close(rdfd);
}

static void drain_task_read_devurandom(struct sysdrain* drain)
{
	message(VERBOSITY_1, "drain_task_read_devurandom");
	drain_task_read(drain, "/dev/urandom");
}

static void drain_task_read_devzero(struct sysdrain* drain)
{
	message(VERBOSITY_1, "drain_task_read_devzero");
	drain_task_read(drain, "/dev/zero");
}

static void drain_task_copy(struct sysdrain* drain)
{
	message(VERBOSITY_1, "drain_task_copy");
	__u64 slot_a;
	__u64 slot_b;
	do {
		slot_a = rand_nonuniform_range(0, drain->d_end);
		slot_b = rand_nonuniform_range(0, drain->d_end);
	} while (slot_a == slot_b);
	
	__u64 slot_asz = drain->d_szslots[slot_a];
	__u64 slot_bsz = drain->d_szslots[slot_b];
	
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
	__u64 fromsz = min(slot_asz, slot_bsz);
	__u64 tosz = max(slot_asz, slot_bsz);
#pragma GCC diagnostic pop
	
	char* from = drain->d_ptrslots[slot_asz < slot_bsz? slot_a: slot_b];
	char* to = drain->d_ptrslots[slot_asz < slot_bsz? slot_b: slot_a];
	
	for (__u64 offset = 0; offset < tosz; offset += fromsz) {
		memcpy(to + offset, from, fromsz);
	}
}

static void drain_task_memfrob(struct sysdrain* drain)
{
	message(VERBOSITY_1, "drain_task_memfrob");
	__u64 slot = rand_nonuniform_range(0, drain->d_end);
	__u64 slotsz = drain->d_szslots[slot];
	memfrob(drain->d_ptrslots[slot], slotsz);
}

static void prepare(struct sysdrainoptions* sdopts)
{
	struct sysinfo sysi;
	sysinfo(&sysi);
	
	double chfactor = sdopts->so_targetpercent / 100.0;
	
	sdopts->so_targetsz = (__u64)(sysi.freeram * chfactor);
	sdopts->so_ceilsz = (__u64)(sysi.freeram * (chfactor + 0.1));
	sdopts->so_floorsz = (__u64)(sysi.freeram * (chfactor - 0.1));
	
	message(VERBOSITY_1, "free ram: %lu", sysi.freeram);
	message(VERBOSITY_1, "percent request: %llu%%", sdopts->so_targetpercent);
	message(VERBOSITY_1, "drain target: %llu", sdopts->so_targetsz);
	message(VERBOSITY_1, "drain ceiling: %llu", sdopts->so_ceilsz);
	message(VERBOSITY_1, "drain floor: %llu", sdopts->so_floorsz);
}

static void handle_more_memory(struct sysdrain* drain,
	const struct sysdrainoptions* const sdopts, __u64 steps)
{
	while (drain->d_memsz < sdopts->so_ceilsz && steps > 0) {
		if (drain->d_end + 1 > drain->d_slots) {
			error("out of slots");
		}
		
		__u64 new_slot = drain->d_end + 1;
		__u64 new_slotsz = 1 << rand_nonuniform_range(
			SYSDRAIN_MINRANDSHIFT, SYSDRAIN_MAXRANDSHIFT);
		
		drain->d_ptrslots[new_slot] = calloc(1, new_slotsz);
		if (drain->d_ptrslots[new_slot]) {
			drain->d_szslots[new_slot] = new_slotsz;
			
			drain->d_end += 1;
			drain->d_memsz += new_slotsz;
			
			message(VERBOSITY_2, "slot %llu filled with %llu bytes",
				new_slot, new_slotsz);
		} else {
			message(VERBOSITY_2, "failed to satisfy request for %llu bytes for slot %llu",
				new_slotsz, new_slot);
		}
		
		steps--;
	}
}

static void handle_less_memory(struct sysdrain* drain,
	const struct sysdrainoptions* const sdopts, __u64 steps)
{
	while (sdopts->so_floorsz < drain->d_memsz && steps > 0) {
		__u64 slot = rand_nonuniform_range(0, drain->d_end);
		__u64 slotsz = drain->d_szslots[slot];
		
		free(drain->d_ptrslots[slot]);
		
		drain->d_ptrslots[slot] = drain->d_ptrslots[drain->d_end];
		drain->d_ptrslots[drain->d_end] = NULL;
		
		drain->d_szslots[slot] = drain->d_szslots[drain->d_end];
		drain->d_szslots[drain->d_end] = 0;
		
		drain->d_end -= 1;
		drain->d_memsz -= slotsz;
		
		message(VERBOSITY_2, "%llu bytes freed from slot %llu",
			slotsz, slot);
		
		steps--;
	}
}

static void handle_memory(struct sysdrain* drain,
	const struct sysdrainoptions* const sdopts)
{
	int situation_nominal = (
		drain->d_memsz >= sdopts->so_floorsz &&
		drain->d_memsz <= sdopts->so_ceilsz
	);
	if (!situation_nominal || rand_nonuniform_range(0, 8) == 0) {
		if (drain->d_memsz < sdopts->so_ceilsz)
			handle_more_memory(drain, sdopts, 32);
		else
			handle_less_memory(drain, sdopts, 64);
	}
	message(VERBOSITY_2, "drained bytes: %llu", drain->d_memsz);
}

static void handle_workload(struct sysdrain* drain,
	const struct sysdrainoptions* const sdopts)
{
	drain_task drain_tasks[] = {
		drain_task_memset0,
		drain_task_read_devurandom,
		drain_task_read_devzero,
		drain_task_copy,
		drain_task_memfrob
	};
	
	(void)sdopts;
	
	int ndrain_tasks = sizeof(drain_tasks) / sizeof(drain_task);
	int tasknr = rand_nonuniform_range(0, ndrain_tasks - 1);
	drain_task task = drain_tasks[tasknr];
	
	task(drain);
}

static void drain(const struct sysdrainoptions* const sdopts)
{
	struct sysdrain drain;
	memset(&drain, 0, sizeof(drain));
	
	drain.d_slots = (__u64)(
		sdopts->so_ceilsz / (1 << SYSDRAIN_MINRANDSHIFT)
	) + 1;
	
	drain.d_ptrslots = calloc(drain.d_slots, sizeof(*drain.d_ptrslots));
	drain.d_szslots = calloc(drain.d_slots, sizeof(*drain.d_szslots));
	if (!drain.d_ptrslots || !drain.d_szslots) {
		error("failed to allocate slots");
	}
	drain.d_end = 0;
	
	srand(sdopts->so_seed);
	
	for (;;) {
		handle_memory(&drain, sdopts);
		handle_workload(&drain, sdopts);
	}
}

static void usage(const char* const exe, FILE* const dest)
{
	fprintf(dest,
		"\nUsage: %s [-%s]\n"
		"\nexample: %s\n\n"
		" -h          print this message (to stdout) and quit\n"
		" -v          be more verbose\n"
		" -e          turn warnings into errors\n"
		" -s <int>    random seed\n"
		" -m <int>    percent of free RAM to drain (m >= 10, m <= 90)\n"
		"\n", exe, SYSDRAIN_OPTIONS, exe);
	
	exit(dest == stderr? EXIT_FAILURE: EXIT_SUCCESS);
}

int main(int argc, char* argv[])
{
	if (argc == 0)
		usage("sysdrain", stderr);
	
	struct sysdrainoptions sdopts;
	memset(&sdopts, 0, sizeof(sdopts));
	
	sdopts.so_seed = time(NULL);
	sdopts.so_targetpercent = 70;
	
	__u64 optarglen;
	int option;
	while ((option = getopt(argc, argv, SYSDRAIN_OPTIONS)) != EOF) {
		switch (option) {
			case 'h':
				usage(argv[0], stdout);
				break;
			case 'v':
				hostprog_verbosity++;
				break;
			case 'e':
				hostprog_werror = 1;
				break;
			case 's':
				opt_strtolx(ul, option, optarg, sdopts.so_seed);
				break;
			case 'm':
				optarglen = strlen(optarg);
				if (optarglen > 2 && optarg[optarglen - 1] == '%') {
					optarg[optarglen - 1] = '\0';
				}
				opt_strtolx(ul, option, optarg, sdopts.so_targetpercent);
				if (sdopts.so_targetpercent < 10 || sdopts.so_targetpercent > 90)
					error("invalid memory drain request: %llu%%\n",
						sdopts.so_targetpercent);
				break;
			default:
				/* Ignore it.
				 */
				warning("unrecognized option -%c", option);
				break;
		}
	}
	
	prepare(&sdopts);
	drain(&sdopts);
	
	exit(EXIT_SUCCESS);
}
