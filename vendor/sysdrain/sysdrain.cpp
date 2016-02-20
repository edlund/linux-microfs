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

#if defined(_WIN32) || defined(_WIN64) || defined(__WINRT__)
#define ENV_WINDOWS
#ifdef _MSC_VER
#define ENV_WINDOWS_MSC
#endif
#include <sdkddkver.h>
#else
#define ENV_UNIX
#endif

#include <algorithm>
#include <list>
#include <mutex>
#include <new>
#include <random>
#include <thread>

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
#include <inttypes.h>
#include <unistd.h>

#ifdef ENV_UNIX
#include <sys/stat.h>
#include <sys/sysinfo.h>
#else
#include <windows.h>
#endif

#define SYSDRAIN_MINRANDSHIFT 10
#define SYSDRAIN_MAXRANDSHIFT 22

#define SYSDRAIN_OPTIONS "hves:m:M:t:"

static std::size_t g_verbosity = 0;
static std::size_t g_werror = 0;
static std::mutex g_print_mutex;
static std::mutex g_rand_mutex;

static int rand_uniform_range(int min, int max)
{
	static std::random_device rd;
	static std::mt19937 mt(rd());
	
	std::lock_guard<std::mutex> lock(g_rand_mutex);
	std::uniform_int_distribution<int> dist(min, max);
	
	return dist(mt);
}

#define do_error(Exit, CanExit, ...) \
	do { \
		g_print_mutex.lock(); \
		std::fprintf(stderr, "error: "); \
		std::fprintf(stderr, __VA_ARGS__); \
		std::fprintf(stderr, " (%s:%d)", __FILE__, __LINE__); \
		std::fprintf(stderr, "\n"); \
		g_print_mutex.unlock(); \
		if (CanExit) \
			Exit; \
	} while (0)

#define error(...) do_error(exit(EXIT_FAILURE), 1, __VA_ARGS__)

#define do_warning(Exit, CanExit, ...) \
	do { \
		g_print_mutex.lock(); \
		std::fprintf(stderr, "warning: "); \
		std::fprintf(stderr, __VA_ARGS__); \
		std::fprintf(stderr, " (%s:%d)", __FILE__, __LINE__); \
		std::fprintf(stderr, "\n"); \
		g_print_mutex.unlock(); \
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
			g_print_mutex.lock(); \
			std::fprintf(stdout, __VA_ARGS__); \
			std::fprintf(stdout, "\n"); \
			g_print_mutex.unlock(); \
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


static inline void* memrfrob(void *s, size_t n)
{
	auto p = reinterpret_cast<char*>(s);
	while (n-- > 0)
		*p++ ^= rand_uniform_range(0, 255);
	return s;
}

struct sysdrain {
	/* Number of allocated pointer slots. */
	std::uint64_t d_slots;
	/* Pointers to allocated memory. */
	char** d_ptrslots;
	/* Size of pointers in %d_ptrslots. */
	std::uint64_t* d_szslots;
	/* The last occupied index of %d_ptrslots. */
	std::uint64_t d_end;
	/* Total size of allocated memory. */
	std::uint64_t d_memsz;
	/* Local %so_targetsz. */
	std::uint64_t d_lcl_targetsz;
	/* Local %so_ceilsz. */
	std::uint64_t d_lcl_ceilsz;
	/* Local %so_floorsz. */
	std::uint64_t d_lcl_floorsz;
};

struct sysdrainoptions {
	/* Random seed. */
	unsigned int so_seed;
	/* Number of threads to spawn. */
	std::uint32_t so_threads;
	/* User requested drain percentage. */
	std::uint64_t so_targetpercent;
	/* Target size based on given %so_targetpercent. */
	std::uint64_t so_targetsz;
	/* Approx. max allocated memory. */
	std::uint64_t so_ceilsz;
	/* Approx. min allocated memory. */
	std::uint64_t so_floorsz;
};

typedef void (*drain_task)(struct sysdrain&);

static void drain_task_memset(struct sysdrain& drain, int v)
{
	message(VERBOSITY_2, "drain_task_memset");
	std::uint64_t slot = rand_uniform_range(0, drain.d_end);
	std::uint64_t slotsz = drain.d_szslots[slot];
	std::memset(drain.d_ptrslots[slot], v, slotsz);
}


static void drain_task_memset0(struct sysdrain& drain)
{
	message(VERBOSITY_2, "drain_task_memset0");
	drain_task_memset(drain, 0);
}

static void drain_task_memsetX(struct sysdrain& drain)
{
	message(VERBOSITY_2, "drain_task_memsetX");
	drain_task_memset(drain, rand_uniform_range(0, 255));
}

#ifdef ENV_UNIX

static void drain_task_read(struct sysdrain& drain, const char* path)
{
	int rdfd = ::open(path, O_RDONLY, 0);
	if (rdfd < 0) {
		error("failed to open path \"%s\": %s", path, ::strerror(errno));
	}
	std::uint64_t slot = rand_uniform_range(0, drain.d_end);
	std::uint64_t slotsz = drain.d_szslots[slot];
	if (::read(rdfd, drain.d_ptrslots[slot], slotsz) < 0) {
		error("failed to read \"%s\": %s", path, ::strerror(errno));
	}
	::close(rdfd);
}

static void drain_task_read_devurandom(struct sysdrain& drain)
{
	message(VERBOSITY_2, "drain_task_read_devurandom");
	drain_task_read(drain, "/dev/urandom");
}

static void drain_task_read_devzero(struct sysdrain& drain)
{
	message(VERBOSITY_2, "drain_task_read_devzero");
	drain_task_read(drain, "/dev/zero");
}

#endif

static void drain_task_copy(struct sysdrain& drain)
{
	message(VERBOSITY_2, "drain_task_copy");
	std::uint64_t slot_a;
	std::uint64_t slot_b;
	do {
		slot_a = rand_uniform_range(0, drain.d_end);
		slot_b = rand_uniform_range(0, drain.d_end);
	} while (slot_a == slot_b);
	
	std::uint64_t slot_asz = drain.d_szslots[slot_a];
	std::uint64_t slot_bsz = drain.d_szslots[slot_b];
	
	std::uint64_t fromsz = std::min(slot_asz, slot_bsz);
	std::uint64_t tosz = std::max(slot_asz, slot_bsz);
	
	char* from = drain.d_ptrslots[slot_asz < slot_bsz? slot_a: slot_b];
	char* to = drain.d_ptrslots[slot_asz < slot_bsz? slot_b: slot_a];
	
	for (std::uint64_t offset = 0; offset < tosz; offset += fromsz) {
		memcpy(to + offset, from, fromsz);
	}
}

static void drain_task_memrfrob(struct sysdrain& drain)
{
	message(VERBOSITY_2, "drain_task_memrfrob");
	std::uint64_t slot = rand_uniform_range(0, drain.d_end);
	std::uint64_t slotsz = drain.d_szslots[slot];
	memrfrob(drain.d_ptrslots[slot], slotsz);
}

static std::uint32_t nthreads(const struct sysdrainoptions& sdopts)
{
	if (sdopts.so_threads)
		return sdopts.so_threads;
	else {
		std::uint32_t threads = std::thread::hardware_concurrency();
		if (!threads) {
			warning("failed to get hardware concurrency, using 1 thread");
			return 1;
		} else
			return threads;
	}
}

static void prepare(struct sysdrainoptions& sdopts)
{
	sdopts.so_threads = nthreads(sdopts);
	
	auto print_percentage = sdopts.so_targetsz == 0;
	auto free_ram = 0ULL;
	
#ifdef ENV_UNIX
	struct sysinfo sysi;
	::sysinfo(&sysi);
	free_ram = sysi.freeram;
#else
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	::GlobalMemoryStatusEx(&status);
	free_ram = status.ullTotalPhys;
#endif
	message(VERBOSITY_1, "\n");
	message(VERBOSITY_1, "free ram: %llu", free_ram);
	
	if (sdopts.so_targetsz) {
		sdopts.so_targetsz = free_ram - sdopts.so_targetsz;
		std::uint64_t diff = (std::uint64_t)(sdopts.so_targetsz / 10.0);
		sdopts.so_ceilsz = sdopts.so_targetsz + diff;
		sdopts.so_floorsz = sdopts.so_targetsz - diff;
	} else {
		double chfactor = sdopts.so_targetpercent / 100.0;
		sdopts.so_targetsz = (std::uint64_t)(free_ram * chfactor);
		sdopts.so_ceilsz = (std::uint64_t)(free_ram * (chfactor + 0.1));
		sdopts.so_floorsz = (std::uint64_t)(free_ram * (chfactor - 0.1));
	}
	
	if (print_percentage) {
		message(VERBOSITY_1, "percent request: %" PRIu64 "%%",
			sdopts.so_targetpercent);
	}
	message(VERBOSITY_1, "drain target: %" PRIu64, sdopts.so_targetsz);
	message(VERBOSITY_1, "drain ceiling: %" PRIu64, sdopts.so_ceilsz);
	message(VERBOSITY_1, "drain floor: %" PRIu64, sdopts.so_floorsz);
}

static void handle_more_memory(struct sysdrain& drain,
	const struct sysdrainoptions& sdopts, std::uint64_t steps)
{
	(void)sdopts;
	
	while (drain.d_memsz < drain.d_lcl_ceilsz && steps > 0) {
		if (drain.d_end + 1 > drain.d_slots) {
			error("out of slots");
		}
		
		std::uint64_t new_slot = drain.d_end + 1;
		std::uint64_t new_slotsz = 1 << rand_uniform_range(
			SYSDRAIN_MINRANDSHIFT,
			SYSDRAIN_MAXRANDSHIFT
		);
		
		try {
			drain.d_ptrslots[new_slot] = new char[new_slotsz];
			drain.d_szslots[new_slot] = new_slotsz;
			
			drain.d_end += 1;
			drain.d_memsz += new_slotsz;
			
			message(VERBOSITY_2, "slot %" PRIu64 " filled with %" PRIu64 " bytes",
				new_slot, new_slotsz);
		}
		catch (std::exception& e) {
			message(VERBOSITY_1, "failed to satisfy request for %" PRIu64 " bytes"
				" for slot %" PRIu64 " (%s)", new_slotsz, new_slot, e.what());
		}
		
		steps--;
	}
}

static void handle_less_memory(struct sysdrain& drain,
	const struct sysdrainoptions& sdopts, std::uint64_t steps)
{
	(void)sdopts;
	
	while (drain.d_lcl_floorsz < drain.d_memsz && steps > 0) {
		std::uint64_t slot = rand_uniform_range(0, drain.d_end);
		std::uint64_t slotsz = drain.d_szslots[slot];
		
		delete [] drain.d_ptrslots[slot];
		
		drain.d_ptrslots[slot] = drain.d_ptrslots[drain.d_end];
		drain.d_ptrslots[drain.d_end] = NULL;
		
		drain.d_szslots[slot] = drain.d_szslots[drain.d_end];
		drain.d_szslots[drain.d_end] = 0;
		
		drain.d_end -= 1;
		drain.d_memsz -= slotsz;
		
		message(VERBOSITY_2, "%" PRIu64 " bytes freed from slot %" PRIu64,
			slotsz, slot);
		
		steps--;
	}
}

static void handle_memory(struct sysdrain& drain,
	const struct sysdrainoptions& sdopts)
{
	int situation_nominal = (
		drain.d_memsz >= drain.d_lcl_floorsz &&
		drain.d_memsz <= drain.d_lcl_ceilsz
	);
	if (!situation_nominal || rand_uniform_range(0, 4) == 0) {
		if (drain.d_memsz < drain.d_lcl_ceilsz)
			handle_more_memory(drain, sdopts, 64);
		else
			handle_less_memory(drain, sdopts, 64);
	}
	message(VERBOSITY_2, "drained bytes: %" PRIu64, drain.d_memsz);
}

static void handle_workload(struct sysdrain& drain,
	const struct sysdrainoptions& sdopts)
{
	drain_task drain_tasks[] = {
#ifdef ENV_UNIX
		drain_task_memset0,
		drain_task_memsetX,
		drain_task_read_devurandom,
		drain_task_read_devzero,
		drain_task_copy,
		drain_task_memrfrob
#else
		drain_task_memset0,
		drain_task_memsetX,
		drain_task_copy,
		drain_task_memrfrob
#endif
	};
	
	(void)sdopts;
	
	auto ndrain_tasks = sizeof(drain_tasks) / sizeof(drain_task);
	auto tasknr = rand_uniform_range(0, ndrain_tasks - 1);
	drain_task task = drain_tasks[tasknr];
	
	task(drain);
}

static void drain(const struct sysdrainoptions& sdopts)
{
	struct sysdrain drain;
	std::memset(&drain, 0, sizeof(drain));
	
	drain.d_lcl_targetsz = sdopts.so_targetsz / sdopts.so_threads;
	drain.d_lcl_ceilsz = sdopts.so_ceilsz / sdopts.so_threads;
	drain.d_lcl_floorsz = sdopts.so_floorsz / sdopts.so_threads;
	
	message(VERBOSITY_1, "\n[drainaddr=%p] thread spawned\n"
			"\tdrain target: %" PRIu64 "\n"
			"\tdrain ceiling: %" PRIu64 "\n"
			"\tdrain floor: %" PRIu64 "\n",
		(void*)&drain,
		drain.d_lcl_targetsz,
		drain.d_lcl_ceilsz,
		drain.d_lcl_floorsz
	);
	
	drain.d_slots = static_cast<std::uint64_t>((
		drain.d_lcl_ceilsz / (1 << SYSDRAIN_MINRANDSHIFT)
	) + 1);
	
	drain.d_ptrslots = new char*[drain.d_slots];
	drain.d_szslots = new std::uint64_t[drain.d_slots];
	if (!drain.d_ptrslots || !drain.d_szslots) {
		do_error(pthread_exit(NULL), 1, "failed to allocate slots");
	}
	drain.d_end = 0;
	
	std::srand(sdopts.so_seed);
	
	for (;;) {
		handle_memory(drain, sdopts);
		handle_workload(drain, sdopts);
	}
}

static void usage(const char* const exe, std::FILE* const dest)
{
	std::fprintf(dest,
		"\nUsage: %s [-%s]\n"
		"\nexample: %s\n\n"
		" -h        print this message (to stdout) and quit\n"
		" -v        be more verbose\n"
		" -e        turn warnings into errors\n"
		" -s <int>  random seed\n"
		" -m <int>  percent of free RAM to drain (m >= 10, m <= 90)\n"
		" -M <int>  amount of RAM that should be left free\n"
		" -t <int>  number of threads to use\n"
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
	
	std::uint64_t optarglen;
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
					error("invalid memory drain request: %" PRIu64 "%%\n",
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
	
	prepare(sdopts);
	
	try {
		std::list<std::thread*> threads;
		for (std::uint32_t i = 0; i < sdopts.so_threads; ++i)
			threads.push_back(new std::thread(drain, sdopts));
		for (auto it : threads)
			it->join();
	}
	catch (std::exception& e) {
		error("%s", e.what());
	}
	
	std::exit(EXIT_SUCCESS);
}

