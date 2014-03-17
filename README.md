
# microfs - Minimally Improved Compressed Read Only File System

https://github.com/edlund/linux-microfs

    microfs is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU General Public License for more details.

`microfs` is basically a reimplementation of `cramfs` with a few new
features added which are not backwards compatible. It is mostly
implemented to serve as an exercise for its author and might not
be particularly useful in any real world scenario. If you are
looking for a `cramfs` replacement, you probably want to consider
`squashfs` (which is an excellent read only file system). With that
said, you might still find `microfs` interesting, particularly if
you appreciate `cramfs` for its awesome simplicity.

The difference between `cramfs` and `microfs` lies in that `microfs`

 * Always store metadata as little endian and convert data to the
   endianess of the host when it is necessary/required.
 * Support configurable block sizes (ranging from 512 bytes up to
   1 megabyte).
 * Support image sizes larger than 272 mb (the upper limit is
   2^32 bytes).
 * Support slightly longer file names (3 extra bytes).
 * Support ctime/mtime values for inodes (although all files will
   share the same value which is when the image was created).
 * Support different compression settings for zlib when the image
   is created.
 * Will try to uncompress data from the buffer heads directly
   to the page cache pages if it is possible.
 * Will have different read buffers and zlib streams for
   different mounted images. (`microfs` will therefore always use
   more memory than `cramfs`.)
 * Does not support file holes by simply skipping the zero bytes
   like `cramfs`, instead it will treat file holes as any other data
   and compress it.
 * Split the image into four main parts; 1) superblock, 2) inodes
   and dentries, 3) block pointers and 4) the compressed blocks.
   See the section "image format" for more information.

## Licensing

Most of `microfs` is GPL v.2, just like the Linux kernel. There
are however a few utilities which might be useful outside of
the project and are licensed under the new BSD license. Check
the copyright and license notice at the top of the file to know
what license a specific file is under. GPL v.2 will apply if no
license is explicitly specified in a file.

## Linux kernel compatibility

`microfs` is mainly focused on supporting a small number of
kernel versions for popular distrubutions in the debian family.
This is mostly because the upstream kernel is such a fast moving
target that it would require extra effort to develop against.

Supported kernels at the time of writing:

 * Ubuntu: `3.11.0-x-generic` (13.10, "Saucy Salamander")
 * Debian: `3.12-0.bpo.x-486` (7, "Wheezy")
 * Debian: `3.12-0.bpo.x-amd64` (7, "Wheezy")
 * Raspbian: `3.10.25+`

It may well work with other versions and other dists, but that
has not been tested.

## Image format

`microfs` images has four main parts:

 * `super block`
 * `inodes/dentries`
 * `block pointers`
 * `compressed data blocks`

The main difference compared to `cramfs` is that the block
pointers are placed in their own section rather than being
placed just before the compressed data blocks that they point
to. The idea is that this change should allow `microfs` to use
its buffers more efficiently. The downside is that each regular
file and symlink requires one extra block pointer with this
setup.

## Building microfs

It is presumed that you are planning to build `microfs` on a
GNU/Linux system which can compile the Linux kernel. Adding
to the requirements implied by this assumption `microfs` will
also need the following things in order to compile successfully:

 * gcc (>=4.7), http://gcc.gnu.org/
 * GNU make (>=3.81), http://www.gnu.org/software/make/
 * check unit test framework (>=0.9.8), http://check.sourceforge.net/
 * zlib (>=1.2.7), http://www.zlib.net/
 * python (>=2.7), http://www.python.org/
 * cramfs-tools (>=1.1), http://sourceforge.net/projects/cramfs/ (opt)
 * squashfs-tools (>=4.2), http://squashfs.sourceforge.net/ (opt)
 * inotify-tools (>=3.14), https://github.com/rvoicilas/inotify-tools (opt)
 * tmpfs (>=3.11.0), https://www.kernel.org/ (opt)

Once the build environment is set up it is sufficient to run

    $ make

from the root source directory in order to build the lkm and the
hostprogs.

Use the make command line argument `DEBUG=1` or the combination
`DEBUG=1 DEBUG_SPAM=1` to do a debug build. Please note that a
`debug spam` lkm will spam your syslog with messages (hence
the name), it can however be very useful when dealing with a
bug which is reproducible with a small set of files (or few
operations).

## Testing microfs

### Reproducible "randomness"

Pseudorandomness plays a very important role in the functional
testing of `microfs`. Most of the data used for the tests is
pseudorandomly generated and can be regenerated at any time
if the seed that was used in the first place is known. This
should provide a couple of major advantages:

 * microfs is tested on a wide variety of pseudorandom trees
   of files and directories.
 * Test sessions should be reproducible even if the actual
   data that is used is pseudorandom.
 * Random data will compress very badly, meaning that the test
   suite also can serve as a stress test of sorts.

### Remote checks

`microfs` supports "remote checks", which makes it easy to run
the test suite on some machine other than your primary
workstation. Using a virtual machine is probably the best
solution as it is very easy to reset and easier to debug if
(when) something goes terribly wrong.

An example:

    $ make remotecheck \
    >     REMOTEHOST="localhost" \
    >     REMOTEPORT="2222" \
    >     REMOTEUSER="erik" \
    >     REMOTEDEST="~" \
    >     REMOTEMKARGS="CHECKARGS='-r 1387916272'"

Port 2222 on localhost is forwarded to a virtual machine with
a ssh server.

Of the given arguments, `REMOTEMKARGS` is extra interesting.
Use it to pass command line arguments to the remote make
invocation. In the above example `test.sh` is seeded with
the value `1387916272`. Another example of using `REMOTEMKARGS`
could be to set it to `DEBUG=1 DEBUG_SPAM=1` in order to do
a debug build.

### Stress/Load testing

Use the flag "-S" for test.sh in order to have it run a stress
and/or load test on the lkm. The idea is to try to test that the
lkm works as it should even when the system is under heavy load
and have little RAM to work with.

The contrast is an ordinary test run which pretty much allows
the lkm to run under ideal circumstances. Hopefully there will
not be too many bugs hiding between the extremes.

    $ make remotecheck \
    >     REMOTEHOST="localhost" \
    >     REMOTEPORT="2222" \
    >     REMOTEUSER="erik" \
    >     REMOTEDEST="~" \
    >     REMOTEMKARGS="CHECKARGS='-r 1389994471 -S'"

### "Quick" tests

Use the flag "-Q" for test.sh in order to limit the block
sizes tested to 512, 4096, 131072 and 1048576 (otherwise test.sh
will use block sizes 512, 1024, 2048, 4096, 8192, 16384, 32768,
65536, 131072 and 1048576).

## Performance

No filesystem README would be complete without a section about
performance. `microfs` is shipped with a couple of simple benchmark
tools which can be used to compare the performance of `microfs`,
`cramfs` and `squashfs` for common filesystem operations.

The tests are limited to image sizes that `cramfs` can handle,
for image sizes larger than 272 mb `microfs` will quickly loose
ground to `sqaushfs` which will easily outperform it for big
images.

### Fairness

Pseudorandomness is once again important; it is used to
generate the data which is used for the performance tests
and by `frd`, the tool which is responsible for reading data
from files. The idea is that exactly the same read operations
should be done for all filesystems in order to avoid skewed
results and to allow benchmarks to be reproduced if the seed
is known.

`squashfs` images are created with `-noI` so that it does not
have to spend time decompressing the inode table when both
`cramfs` and `microfs` store inodes uncompressed.

Obviously every block size tested larger than `PAGE_CACHE_SIZE`
will mostly be interesting to compare `microfs` with `squashfs`,
`cramfs` will still use `PAGE_CACHE_SIZE` sized blocks.

The result presented by the benchmark tools is the average
time that a test took over `N` passes. The idea is that the
average should give a more fair representation of the performance
of the filesystem than a single pass would.

Still, with all this said, it is still probably wise to consider
the bundled benchmark tools to be biased towards making `microfs`
look good, even if that is not the intention.

### Results

A few results from benchmarks performed on a test machine
is included. They are listed below. The command found before
each result listing can be used to run the benchmark that
generated the result in question.

The test machine is a VirtualBox VM running Ubuntu 13.10
x86-64 with 1024MB RAM and 1 virtual processor.

The host for the test machine is an Acer Aspire 7741G with
4096MB RAM and an Intel Core i5 CPU M 450 @ 2.40GHz with
4 cores running Ubuntu 12.04 x86-64.

#### 19 directories, 337 files, small files

##### Block size 4096 bytes, 10 passes

Command:
    $ tools/rofsbench.sh -n 10 -r 1387916272 -b 4096 -w /tmp/perf/
Result:
    Test 0: list all recursively
    Command: `ls -lAR /tmp/perf/tmpfs/mnt`
        microfs:   real=0.1320    sys=0.0070    user=0.0030
        squashfs:  real=0.1270    sys=0.0050    user=0.0040
        cramfs:    real=0.1380    sys=0.0060    user=0.0010

    Test 1: find all
    Command: `find /tmp/perf/tmpfs/mnt`
        microfs:   real=0.0960    sys=0.0000    user=0.0000
        squashfs:  real=0.0880    sys=0.0000    user=0.0000
        cramfs:    real=0.0950    sys=0.0000    user=0.0000

    Test 2: find files
    Command: `find /tmp/perf/tmpfs/mnt -type f`
        microfs:   real=0.0860    sys=0.0000    user=0.0000
        squashfs:  real=0.0890    sys=0.0000    user=0.0000
        cramfs:    real=0.0820    sys=0.0010    user=0.0000

    Test 3: seq access, seq reading
    Command: `tools/frd -e -s 1387916272 -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.7490    sys=0.4960    user=0.0040
        squashfs:  real=0.9820    sys=0.5060    user=0.0030
        cramfs:    real=0.6350    sys=0.3880    user=0.0020

    Test 4: seq access, stat-only
    Command: `tools/frd -e -s 1387916272 -N -i /tmp/perf/tmpfs/tmp/all-paths.txt`
        microfs:   real=0.0090    sys=0.0000    user=0.0000
        squashfs:  real=0.0110    sys=0.0000    user=0.0000
        cramfs:    real=0.0120    sys=0.0000    user=0.0000

    Test 5: rand access, seq reading
    Command: `tools/frd -e -s 1387916272 -R -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.7530    sys=0.5060    user=0.0010
        squashfs:  real=0.9870    sys=0.5060    user=0.0000
        cramfs:    real=0.6320    sys=0.3980    user=0.0010

    Test 6: rand access, stat-only
    Command: `tools/frd -e -s 1387916272 -R -N -i /tmp/perf/tmpfs/tmp/all-paths.txt`
        microfs:   real=0.0100    sys=0.0000    user=0.0000
        squashfs:  real=0.0090    sys=0.0000    user=0.0000
        cramfs:    real=0.0130    sys=0.0000    user=0.0000

    Test 7: seq access, rand reading
    Command: `tools/frd -e -s 1387916272 -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.8070    sys=0.5540    user=0.0060
        squashfs:  real=1.0600    sys=0.5550    user=0.0020
        cramfs:    real=0.7480    sys=0.5060    user=0.0030

    Test 8: rand access, rand reading
    Command: `tools/frd -e -s 1387916272 -R -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.8160    sys=0.5630    user=0.0030
        squashfs:  real=1.0570    sys=0.5440    user=0.0020
        cramfs:    real=0.7490    sys=0.5190    user=0.0050

##### Block size 131072 bytes, 10 passes

Command:
    $ tools/rofsbench.sh -n 10 -r 1387916272 -b 131072 -w /tmp/perf/
Result:
    Test 0: list all recursively
    Command: `ls -lAR /tmp/perf/tmpfs/mnt`
        microfs:   real=0.1310    sys=0.0100    user=0.0010
        squashfs:  real=0.1350    sys=0.0060    user=0.0050
        cramfs:    real=0.1320    sys=0.0070    user=0.0020

    Test 1: find all
    Command: `find /tmp/perf/tmpfs/mnt`
        microfs:   real=0.0900    sys=0.0000    user=0.0000
        squashfs:  real=0.0840    sys=0.0000    user=0.0000
        cramfs:    real=0.0900    sys=0.0000    user=0.0000

    Test 2: find files
    Command: `find /tmp/perf/tmpfs/mnt -type f`
        microfs:   real=0.0820    sys=0.0000    user=0.0000
        squashfs:  real=0.0870    sys=0.0000    user=0.0000
        cramfs:    real=0.0870    sys=0.0000    user=0.0010

    Test 3: seq access, seq reading
    Command: `tools/frd -e -s 1387916272 -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.5640    sys=0.3600    user=0.0010
        squashfs:  real=0.8120    sys=0.3780    user=0.0000
        cramfs:    real=0.6280    sys=0.3940    user=0.0000

    Test 4: seq access, stat-only
    Command: `tools/frd -e -s 1387916272 -N -i /tmp/perf/tmpfs/tmp/all-paths.txt`
        microfs:   real=0.0110    sys=0.0000    user=0.0000
        squashfs:  real=0.0120    sys=0.0000    user=0.0000
        cramfs:    real=0.0100    sys=0.0000    user=0.0000

    Test 5: rand access, seq reading
    Command: `tools/frd -e -s 1387916272 -R -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.5680    sys=0.3660    user=0.0020
        squashfs:  real=0.8210    sys=0.3870    user=0.0010
        cramfs:    real=0.6340    sys=0.4050    user=0.0020

    Test 6: rand access, stat-only
    Command: `tools/frd -e -s 1387916272 -R -N -i /tmp/perf/tmpfs/tmp/all-paths.txt`
        microfs:   real=0.0130    sys=0.0000    user=0.0000
        squashfs:  real=0.0120    sys=0.0000    user=0.0000
        cramfs:    real=0.0120    sys=0.0000    user=0.0000

    Test 7: seq access, rand reading
    Command: `tools/frd -e -s 1387916272 -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.6010    sys=0.3900    user=0.0030
        squashfs:  real=0.8440    sys=0.3960    user=0.0060
        cramfs:    real=0.7420    sys=0.4960    user=0.0020

    Test 8: rand access, rand reading
    Command: `tools/frd -e -s 1387916272 -R -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.6040    sys=0.3900    user=0.0050
        squashfs:  real=0.8560    sys=0.3950    user=0.0020
        cramfs:    real=0.7420    sys=0.4990    user=0.0060

#### 0 directories, 46 files, large files

##### Block size 4096 bytes, 10 passes

Command:
    $ tools/rofsbench.sh -n 10 -r 1389877799 -b 4096 -w /tmp/perf/
Result:
    Test 0: list all recursively
    Command: `ls -lAR /tmp/perf/tmpfs/mnt`
        microfs:   real=0.1180    sys=0.0030    user=0.0010
        squashfs:  real=0.1220    sys=0.0010    user=0.0000
        cramfs:    real=0.1290    sys=0.0010    user=0.0000

    Test 1: find all
    Command: `find /tmp/perf/tmpfs/mnt`
        microfs:   real=0.0850    sys=0.0000    user=0.0000
        squashfs:  real=0.0830    sys=0.0000    user=0.0000
        cramfs:    real=0.0920    sys=0.0000    user=0.0000

    Test 2: find files
    Command: `find /tmp/perf/tmpfs/mnt -type f`
        microfs:   real=0.0840    sys=0.0000    user=0.0000
        squashfs:  real=0.0850    sys=0.0000    user=0.0000
        cramfs:    real=0.0860    sys=0.0000    user=0.0000

    Test 3: seq access, seq reading
    Command: `tools/frd -e -s 1389877799 -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.7400    sys=0.5210    user=0.0000
        squashfs:  real=0.9980    sys=0.5190    user=0.0010
        cramfs:    real=0.6320    sys=0.3870    user=0.0020

    Test 4: seq access, stat-only
    Command: `tools/frd -e -s 1389877799 -N -i /tmp/perf/tmpfs/tmp/all-paths.txt`
        microfs:   real=0.0110    sys=0.0000    user=0.0000
        squashfs:  real=0.0120    sys=0.0000    user=0.0000
        cramfs:    real=0.0080    sys=0.0000    user=0.0000

    Test 5: rand access, seq reading
    Command: `tools/frd -e -s 1389877799 -R -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.7400    sys=0.4920    user=0.0020
        squashfs:  real=0.9940    sys=0.5100    user=0.0030
        cramfs:    real=0.6250    sys=0.3920    user=0.0000

    Test 6: rand access, stat-only
    Command: `tools/frd -e -s 1389877799 -R -N -i /tmp/perf/tmpfs/tmp/all-paths.txt`
        microfs:   real=0.0120    sys=0.0000    user=0.0000
        squashfs:  real=0.0100    sys=0.0000    user=0.0000
        cramfs:    real=0.0110    sys=0.0000    user=0.0000

    Test 7: seq access, rand reading
    Command: `tools/frd -e -s 1389877799 -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.8110    sys=0.5670    user=0.0030
        squashfs:  real=1.0740    sys=0.5630    user=0.0030
        cramfs:    real=0.7520    sys=0.5020    user=0.0030

    Test 8: rand access, rand reading
    Command: `tools/frd -e -s 1389877799 -R -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.8190    sys=0.5810    user=0.0030
        squashfs:  real=1.0760    sys=0.5940    user=0.0010
        cramfs:    real=0.7510    sys=0.5040    user=0.0010

##### Block size 131072 bytes, 10 passes

Command:
    $ tools/rofsbench.sh -n 10 -r 1389877799 -b 131072 -w /tmp/perf/
Result:
    Test 0: list all recursively
    Command: `ls -lAR /tmp/perf/tmpfs/mnt`
        microfs:   real=0.1210    sys=0.0030    user=0.0000
        squashfs:  real=0.1200    sys=0.0000    user=0.0000
        cramfs:    real=0.1290    sys=0.0000    user=0.0010

    Test 1: find all
    Command: `find /tmp/perf/tmpfs/mnt`
        microfs:   real=0.0840    sys=0.0000    user=0.0000
        squashfs:  real=0.0840    sys=0.0000    user=0.0000
        cramfs:    real=0.0900    sys=0.0000    user=0.0000

    Test 2: find files
    Command: `find /tmp/perf/tmpfs/mnt -type f`
        microfs:   real=0.0850    sys=0.0000    user=0.0000
        squashfs:  real=0.0850    sys=0.0000    user=0.0000
        cramfs:    real=0.0860    sys=0.0000    user=0.0000

    Test 3: seq access, seq reading
    Command: `tools/frd -e -s 1389877799 -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.5590    sys=0.3470    user=0.0010
        squashfs:  real=0.8020    sys=0.3670    user=0.0000
        cramfs:    real=0.6250    sys=0.3870    user=0.0010

    Test 4: seq access, stat-only
    Command: `tools/frd -e -s 1389877799 -N -i /tmp/perf/tmpfs/tmp/all-paths.txt`
        microfs:   real=0.0120    sys=0.0000    user=0.0000
        squashfs:  real=0.0100    sys=0.0000    user=0.0000
        cramfs:    real=0.0130    sys=0.0000    user=0.0000

    Test 5: rand access, seq reading
    Command: `tools/frd -e -s 1389877799 -R -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.5620    sys=0.3560    user=0.0010
        squashfs:  real=0.8030    sys=0.3650    user=0.0020
        cramfs:    real=0.6270    sys=0.3950    user=0.0020

    Test 6: rand access, stat-only
    Command: `tools/frd -e -s 1389877799 -R -N -i /tmp/perf/tmpfs/tmp/all-paths.txt`
        microfs:   real=0.0110    sys=0.0000    user=0.0000
        squashfs:  real=0.0070    sys=0.0000    user=0.0000
        cramfs:    real=0.0120    sys=0.0000    user=0.0000

    Test 7: seq access, rand reading
    Command: `tools/frd -e -s 1389877799 -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.6020    sys=0.3860    user=0.0000
        squashfs:  real=0.8490    sys=0.4010    user=0.0010
        cramfs:    real=0.7510    sys=0.5160    user=0.0000

    Test 8: rand access, rand reading
    Command: `tools/frd -e -s 1389877799 -R -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.6050    sys=0.3910    user=0.0010
        squashfs:  real=0.8500    sys=0.4020    user=0.0010
        cramfs:    real=0.7520    sys=0.4970    user=0.0010

## A small list of mixed TODOs, FIXMEs, WTFs and the sort.

 * Is the pseudorandomness reproducible enough?
 * Add support for `mkrandtree.py` to generate "less random"
   data which will compress acceptably well.
 * Directories are structured and read just like they are in
   `cramfs`. This is probably a good thing, but no thought or
   effort at all has been put into investigating if that is
   true (so far).
 * `microfsmki` and `microfscki` both require high amount of
   RAM available since they both use mmap() to map the whole
   image they are working on (the reason being; it is a
   solution which is easy to implement and easy to understand).
   This is probably not an issue since most workstations have
   plenty of RAM. It is however possible that the memory
   requirement is unreasonable, if so, it should be possible
   to rewrite them.
 * The devtable support seems to be quite buggy when tested
   on real machines (non-virtual).
 * There seems to be something wrong with the performance
   tests, the results seem *very inconsistent* when compared
   between virtual and real machines.

## Ideas for possible future development

 * Implement support for different compression algorithms?
 * Add support for compressed metadata?
 * Implement per-cpu decompression to improve performance for
   parallel I/O?
