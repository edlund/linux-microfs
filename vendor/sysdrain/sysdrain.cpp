/* sysdrain - Drain CPU and RAM for fun and profit
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

#include <algorithm>
#include <new>
#include <random>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <linux/types.h>

#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/types.h>

#define SYSDRAIN_MINRANDSHIFT 10
#define SYSDRAIN_MAXRANDSHIFT 22

#define SYSDRAIN_OPTIONS "hves:m:M:t:"

static std::size_t g_verbosity = 0;
static std::size_t g_werror = 0;
static ::pthread_mutex_t g_print_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline void print_lock()
{
	::pthread_mutex_lock(&g_print_mutex);
}

static inline void print_unlock()
{
	::pthread_mutex_unlock(&g_print_mutex);
}

static int rand_uniform_range(int min, int max)
{
	static std::random_device rd;
	static std::mt19937 mt(rd());
	
	std::uniform_int_distribution<> dist(min, max);
	
	return dist(mt);
}

#define do_error(Exit, CanExit, ...) \
	do { \
		::print_lock(); \
		std::fprintf(stderr, "error: "); \
		std::fprintf(stderr, __VA_ARGS__); \
		std::fprintf(stderr, " (%s:%d)", __FILE__, __LINE__); \
		std::fprintf(stderr, "\n"); \
		::print_unlock(); \
		if (CanExit) \
			Exit; \
	} while (0)

#define error(...) do_error(exit(EXIT_FAILURE), 1, __VA_ARGS__)

#define do_warning(Exit, CanExit, ...) \
	do { \
		::print_lock(); \
		std::fprintf(stderr, "warning: "); \
		std::fprintf(stderr, __VA_ARGS__); \
		std::fprintf(stderr, " (%s:%d)", __FILE__, __LINE__); \
		std::fprintf(stderr, "\n"); \
		::print_unlock(); \
		if (g_werror) \
			do_error(Exit, CanExit, "warnings treated as errors"); \
	} while (0)

#define warning(...) do_warning(exit(EXIT_FAILURE), 1, __VA_ARGS__)

enum {
	VERBOSITY_0 = 0,
	VERBOSITY_1,
	VERBOSITY_2
};

#define message(Level, ...) \
	do { \
		if (g_verbosity >= Level) { \
			::print_lock(); \
			std::fprintf(stdout, __VA_ARGS__); \
			std::fprintf(stdout, "\n"); \
			::print_unlock(); \
		} \
	} while (0)

#define opt_stoi(Opt, Arg, Dest) \
	do { \
		try { \
			Dest = std::stoi(Arg); \
		} \
		catch (std::exception e) { \
			error("arg -%c: %s (%c=%s)", Opt, e.what(), Opt, Arg); \
		} \
	} while (0)

struct sysdrain {
	/* Number of allocated pointer slots. */
	__u64 d_slots;
	/* Pointers to allocated memory. */
	char** d_ptrslots;
	/* Size of pointers in %d_ptrslots. */
	__u64* d_szslots;
	/* The last occupied index of %d_ptrslots. */
	__u64 d_end;
	/* Total size of allocated memory. */
	__u64 d_memsz;
	/* Local %so_targetsz. */
	__u64 d_lcl_targetsz;
	/* Local %so_ceilsz. */
	__u64 d_lcl_ceilsz;
	/* Local %so_floorsz. */
	__u64 d_lcl_floorsz;
};

struct sysdrainoptions {
	/* Random seed. */
	unsigned int so_seed;
	/* Number of threads to spawn. */
	__u32 so_threads;
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
	message(VERBOSITY_2, "drain_task_memset0");
	__u64 slot = rand_uniform_range(0, drain->d_end);
	__u64 slotsz = drain->d_szslots[slot];
	std::memset(drain->d_ptrslots[slot], 0, slotsz);
}

static void drain_task_read(struct sysdrain* drain, const char* path)
{
	int rdfd = ::open(path, O_RDONLY, 0);
	if (rdfd < 0) {
		do_error(::pthread_exit(NULL), 1, "failed to open path \"%s\": %s",
			path, ::strerror(errno));
	}
	__u64 slot = rand_uniform_range(0, drain->d_end);
	__u64 slotsz = drain->d_szslots[slot];
	if (::read(rdfd, drain->d_ptrslots[slot], slotsz) < 0) {
		do_error(::pthread_exit(NULL), 1, "failed to read \"%s\": %s",
			path, ::strerror(errno));
	}
	::close(rdfd);
}

static void drain_task_read_devurandom(struct sysdrain* drain)
{
	message(VERBOSITY_2, "drain_task_read_devurandom");
	drain_task_read(drain, "/dev/urandom");
}

static void drain_task_read_devzero(struct sysdrain* drain)
{
	message(VERBOSITY_2, "drain_task_read_devzero");
	drain_task_read(drain, "/dev/zero");
}

static void drain_task_copy(struct sysdrain* drain)
{
	message(VERBOSITY_2, "drain_task_copy");
	__u64 slot_a;
	__u64 slot_b;
	do {
		slot_a = rand_uniform_range(0, drain->d_end);
		slot_b = rand_uniform_range(0, drain->d_end);
	} while (slot_a == slot_b);
	
	__u64 slot_asz = drain->d_szslots[slot_a];
	__u64 slot_bsz = drain->d_szslots[slot_b];
	
	__u64 fromsz = std::min(slot_asz, slot_bsz);
	__u64 tosz = std::max(slot_asz, slot_bsz);
	
	char* from = drain->d_ptrslots[slot_asz < slot_bsz? slot_a: slot_b];
	char* to = drain->d_ptrslots[slot_asz < slot_bsz? slot_b: slot_a];
	
	for (__u64 offset = 0; offset < tosz; offset += fromsz) {
		memcpy(to + offset, from, fromsz);
	}
}

static void drain_task_memfrob(struct sysdrain* drain)
{
	message(VERBOSITY_2, "drain_task_memfrob");
	__u64 slot = rand_uniform_range(0, drain->d_end);
	__u64 slotsz = drain->d_szslots[slot];
	::memfrob(drain->d_ptrslots[slot], slotsz);
}

static __u32 nthreads(const struct sysdrainoptions* const sdopts)
{
	if (sdopts->so_threads)
		return sdopts->so_threads;
	else {
		__u32 threads = 1;
#define CPU_WARNING \
	warning("failed to get number of online CPUs, using 1 thread")
#ifdef _SC_NPROCESSORS_ONLN
		long conf = ::sysconf(_SC_NPROCESSORS_ONLN);
		if (conf == -1)
			CPU_WARNING;
		else
			threads = (__u32)conf;
#else
#warning "_SC_NPROCESSORS_ONLN not available"
CPU_WARNING;
#endif
#undef CPU_WARNING
		
		return threads;
	}
}

static void prepare(struct sysdrainoptions* sdopts)
{
	sdopts->so_threads = nthreads(sdopts);
	
	struct sysinfo sysi;
	sysinfo(&sysi);
	
	int print_percentage = sdopts->so_targetsz == 0;
	
	if (sdopts->so_targetsz) {
		sdopts->so_targetsz = sysi.freeram - sdopts->so_targetsz;
		__u64 diff = (__u64)(sdopts->so_targetsz / 10.0);
		sdopts->so_ceilsz = sdopts->so_targetsz + diff;
		sdopts->so_floorsz = sdopts->so_targetsz - diff;
	} else {
		double chfactor = sdopts->so_targetpercent / 100.0;
		sdopts->so_targetsz = (__u64)(sysi.freeram * chfactor);
		sdopts->so_ceilsz = (__u64)(sysi.freeram * (chfactor + 0.1));
		sdopts->so_floorsz = (__u64)(sysi.freeram * (chfactor - 0.1));
	}
	
	message(VERBOSITY_1, "\n");
	message(VERBOSITY_1, "free ram: %lu", sysi.freeram);
	if (print_percentage) {
		message(VERBOSITY_1, "percent request: %llu%%",
			sdopts->so_targetpercent);
	}
	message(VERBOSITY_1, "drain target: %llu", sdopts->so_targetsz);
	message(VERBOSITY_1, "drain ceiling: %llu", sdopts->so_ceilsz);
	message(VERBOSITY_1, "drain floor: %llu", sdopts->so_floorsz);
}

static void handle_more_memory(struct sysdrain* drain,
	const struct sysdrainoptions* const sdopts, __u64 steps)
{
	(void)sdopts;
	
	while (drain->d_memsz < drain->d_lcl_ceilsz && steps > 0) {
		if (drain->d_end + 1 > drain->d_slots) {
			do_error(::pthread_exit(NULL), 1, "out of slots");
		}
		
		__u64 new_slot = drain->d_end + 1;
		__u64 new_slotsz = 1 << rand_uniform_range(
			SYSDRAIN_MINRANDSHIFT,
			SYSDRAIN_MAXRANDSHIFT
		);
		
		drain->d_ptrslots[new_slot] = new char[new_slotsz];
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
	(void)sdopts;
	
	while (drain->d_lcl_floorsz < drain->d_memsz && steps > 0) {
		__u64 slot = rand_uniform_range(0, drain->d_end);
		__u64 slotsz = drain->d_szslots[slot];
		
		delete [] drain->d_ptrslots[slot];
		
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
		drain->d_memsz >= drain->d_lcl_floorsz &&
		drain->d_memsz <= drain->d_lcl_ceilsz
	);
	if (!situation_nominal || rand_uniform_range(0, 4) == 0) {
		if (drain->d_memsz < drain->d_lcl_ceilsz)
			handle_more_memory(drain, sdopts, 64);
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
	
	auto ndrain_tasks = sizeof(drain_tasks) / sizeof(drain_task);
	auto tasknr = rand_uniform_range(0, ndrain_tasks - 1);
	drain_task task = drain_tasks[tasknr];
	
	task(drain);
}

static void* drain(void* arg)
{
	auto sdopts = reinterpret_cast<const struct sysdrainoptions* const>(arg);
	
	struct sysdrain drain;
	std::memset(&drain, 0, sizeof(drain));
	
	drain.d_lcl_targetsz = sdopts->so_targetsz / sdopts->so_threads;
	drain.d_lcl_ceilsz = sdopts->so_ceilsz / sdopts->so_threads;
	drain.d_lcl_floorsz = sdopts->so_floorsz / sdopts->so_threads;
	
	message(VERBOSITY_1, "\n[drainaddr=%p] thread spawned\n"
			"\tdrain target: %llu\n"
			"\tdrain ceiling: %llu\n"
			"\tdrain floor: %llu\n",
		(void*)&drain,
		drain.d_lcl_targetsz,
		drain.d_lcl_ceilsz,
		drain.d_lcl_floorsz
	);
	
	drain.d_slots = static_cast<__u64>((
		drain.d_lcl_ceilsz / (1 << SYSDRAIN_MINRANDSHIFT)
	) + 1);
	
	drain.d_ptrslots = new char*[drain.d_slots];
	drain.d_szslots = new __u64[drain.d_slots];
	if (!drain.d_ptrslots || !drain.d_szslots) {
		do_error(pthread_exit(NULL), 1, "failed to allocate slots");
	}
	drain.d_end = 0;
	
	std::srand(sdopts->so_seed);
	
	for (;;) {
		handle_memory(&drain, sdopts);
		handle_workload(&drain, sdopts);
	}
	
	::pthread_exit(NULL);
}

static void usage(const char* const exe, std::FILE* const dest)
{
	std::fprintf(dest,
		"\nUsage: %s [-%s]\n"
		"\nexample: %s\n\n"
		" -h          print this message (to stdout) and quit\n"
		" -v          be more verbose\n"
		" -e          turn warnings into errors\n"
		" -s <int>    random seed\n"
		" -m <int>    percent of free RAM to drain (m >= 10, m <= 90)\n"
		" -M <int>    amount of RAM that should be left free\n"
		" -t <int>    number of threads to use\n"
		"\n", exe, SYSDRAIN_OPTIONS, exe);
	
	std::exit(dest == stderr? EXIT_FAILURE: EXIT_SUCCESS);
}

int main(int argc, char* argv[])
{
	if (argc == 0)
		usage("sysdrain", stderr);
	
	struct sysdrainoptions sdopts;
	std::memset(&sdopts, 0, sizeof(sdopts));
	
	sdopts.so_seed = ::time(NULL);
	sdopts.so_targetpercent = 70;
	
	__u64 optarglen;
	int option;
	while ((option = ::getopt(argc, argv, SYSDRAIN_OPTIONS)) != EOF) {
		switch (option) {
			case 'h':
				usage(argv[0], stdout);
				break;
			case 'v':
				g_verbosity++;
				break;
			case 'e':
				g_werror++;
				break;
			case 's':
				opt_stoi(option, optarg, sdopts.so_seed);
				break;
			case 'm':
				optarglen = std::strlen(optarg);
				if (optarglen > 2 && optarg[optarglen - 1] == '%')
					optarg[optarglen - 1] = '\0';
				opt_stoi(option, optarg, sdopts.so_targetpercent);
				if (sdopts.so_targetpercent < 10 || sdopts.so_targetpercent > 90)
					error("invalid memory drain request: %llu%%\n",
						sdopts.so_targetpercent);
				break;
			case 'M':
				opt_stoi(option, optarg, sdopts.so_targetsz);
				if (sdopts.so_targetsz && sdopts.so_targetsz < 33554432)
					warning("leaving very little free RAM");
				break;
			case 't':
				opt_stoi(option, optarg, sdopts.so_threads);
				break;
			default:
				/* Ignore it.
				 */
				warning("unrecognized option -%c", option);
				break;
		}
	}
	
	prepare(&sdopts);
	
	::pthread_attr_t thread_attr;
	::pthread_attr_init(&thread_attr);
	::pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
	
	auto threads = new pthread_t[sdopts.so_threads];
	if (!threads)
		error("failed to allocate thread handles");
	for (__u32 i = 0; i < sdopts.so_threads; ++i) {
		if (::pthread_create(&threads[i], &thread_attr, drain, (void*)&sdopts))
			error("failed to create thread #%u", i);
	}
	for (__u32 i = 0; i < sdopts.so_threads; ++i) {
		::pthread_join(threads[i], NULL);
	}
	
	std::exit(EXIT_SUCCESS);
}

