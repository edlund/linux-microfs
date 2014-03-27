
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
 * Support ctime/mtime values for VFS inodes by using the ctime
   value for the image stored in the superblock.
 * Will try to uncompress data from the buffer heads directly
   to the page cache pages if it is possible.
 * Will have different read buffers and and decompressors for
   different mounted images. (`microfs` will therefore always use
   more memory than `cramfs`.)
 * Does not support file holes by simply skipping the zero bytes
   like `cramfs`, instead it will treat file holes as any other data
   and compress it.
 * Split the image into four main parts; 1) superblock, 2) inodes
   and dentries, 3) block pointers and 4) the compressed blocks.
   See the section "image format" for more information.

## Licensing

Most of `microfs` is licensed under the GPL. There are however
a few utilities which might be useful outside of the project
and are licensed under the new BSD license. Check the copyright
and license notice at the top of the file to know what license a
specific file is under. GPL will apply if no license is
explicitly specified in a file.

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
 * GNU bash (>=4.2), http://www.gnu.org/software/bash/
 * check unit test framework (>=0.9.8), http://check.sourceforge.net/
 * zlib (>=1.2.7), http://www.zlib.net/
 * python (>=2.7), http://www.python.org/
 * perl (>=5.14), http://www.perl.org/
 * cramfs-tools (>=1.1), http://sourceforge.net/projects/cramfs/ (opt)
 * squashfs-tools (>=4.2), http://squashfs.sourceforge.net/ (opt)
 * inotify-tools (>=3.14), https://github.com/rvoicilas/inotify-tools (opt)
 * tmpfs (>=3.11.0), https://www.kernel.org/ (opt)

Once the build environment is set up it is sufficient to run `make`
from the root source directory to build the lkm and the hostprogs
with zlib support.

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

### Raspberry PI (Raspbian)

The tests should pass without any flags, but there is a risk that
sun might turn into a red giant before any test results are
displayed. It is possible to wait a little less by running a
quick test, reducing the size budget and changing the checksum
algorithm:

    $ make remotecheck \
    >     REMOTEHOST="192.168.x.y" \
    >     REMOTEPORT="22" \
    >     REMOTEUSER="pi" \
    >     REMOTEDEST="~" \
    >     REMOTEMKARGS="CHECKARGS='-r 1389994471 -b 67108864 -C md5sum -Q'"

One should probably avoid stress testing.

## Performance

This section will be updated once support for different
compression algorithms is implemented.

## A small list of mixed TODOs, FIXMEs, WTFs and the sort.

 * Implement support for different compression algorithms.
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
 * Update the benchmark tools.

## Ideas for possible future development

 * Add support for compressed metadata?
 * Implement per-cpu decompression to improve performance for
   parallel I/O?
