
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
   `2^32` bytes).
 * Support files sizes larger than 16 mb (the upper limit is
   `2^32 - 1` bytes).
 * Support slightly longer file names (3 extra bytes).
 * Support ctime/mtime values for VFS inodes by using the ctime
   value for the image stored in the superblock.
 * Will try to uncompress data from the buffer heads directly
   to the page cache pages if it is possible.
 * Will have different read buffers and decompressors for
   different mounted images. (`microfs` will therefore always use
   more memory than `cramfs`.)
 * Does not support file holes by simply skipping the zero bytes
   like `cramfs`, instead it will treat file holes as any other data
   and compress it.
 * Split the image into four main parts; 1) superblock, 2) inodes
   and dentries, 3) block pointers and 4) the compressed blocks.
   See the section "image format" for more information.
 * Support LZ4, LZO and XZ as alternatives to zlib.

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

 * Ubuntu: `3.13.0-x-generic` (14.04, "Trusty Tahr")
 * Debian: `3.13-0.bpo.x-486` (7, "Wheezy")
 * Debian: `3.13-0.bpo.x-amd64` (7, "Wheezy")
 * Raspbian: `3.10-x-rpi` (Raspbian kernel)

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
 * liblz4 (>=r94), http://code.google.com/p/lz4/ (opt)
 * liblzo (>=2.06), http://www.oberhumer.com/opensource/lzo/ (opt)
 * xz-utils (>=5.1), http://tukaani.org/xz/ (opt)
 * python (>=2.7), http://www.python.org/
 * perl (>=5.14), http://www.perl.org/
 * cramfs-tools (>=1.1), http://sourceforge.net/projects/cramfs/ (opt)
 * squashfs-tools (>=4.2), http://squashfs.sourceforge.net/ (opt)
 * inotify-tools (>=3.14), https://github.com/rvoicilas/inotify-tools (opt)
 * tmpfs (>=3.11.0), https://www.kernel.org/ (opt)

Once the build environment is set up it is sufficient to run
`make` from the root source directory to build the lkm and the
hostprogs with zlib support.

It is possible to control which compression libraries are supported
by specifying `LIB_*`-params for make. Available options are:

 * `LIB_ZLIB`
 * `LIB_LZ4`
 * `LIB_LZO`
 * `LIB_XZ`

For example, to add support for LZ4, simply use `make LIB_LZ4=1`.
Please note that zlib support is always compiled for the hostprogs,
so specifying `LIB_ZLIB=0` will only mean that the lkm is compiled
without zlib support.

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
are included. They are listed below. The command found before
each result listing can be used to run the benchmark that
generated the result in question.

The test machine is a RaspberryPI Model-B Rev2, running at
700 MHz with the debian style packaged kernel provided by
Raspbian. Hopefully it will be easy to replicate the results
posted below for anyone interested in doing so.

#### 112 MB; 19 directories, 330 files

##### Block size 4096 bytes, 10 passes

Command:

   linux-microfs/tools/rofsbench.sh -n 10 -r 1387916272 \
        -b 4096 -B 117440512 -w /home/pi/perf/

Result:

    Test 0: list all recursively
    Command: `ls -lAR /home/pi/perf/tmpfs/mnt`
        microfs:  real=0.2050    sys=0.1030    user=0.0480
        squashfs: real=0.1710    sys=0.0690    user=0.0520
        cramfs:   real=0.1610    sys=0.0480    user=0.0690

    Test 1: find all
    Command: `find /home/pi/perf/tmpfs/mnt`
        microfs:  real=0.0600    sys=0.0180    user=0.0050
        squashfs: real=0.0600    sys=0.0090    user=0.0110
        cramfs:   real=0.0600    sys=0.0120    user=0.0080

    Test 2: find files
    Command: `find /home/pi/perf/tmpfs/mnt -type f`
        microfs:  real=0.0700    sys=0.0150    user=0.0150
        squashfs: real=0.0620    sys=0.0190    user=0.0110
        cramfs:   real=0.0630    sys=0.0140    user=0.0140

    Test 3: seq access, seq reading
    Command: `tools/frd -e -s 1387916272 -i /home/pi/perf/tmpfs/tmp/file-paths.txt`
        microfs:  real=9.0320    sys=6.7580    user=0.0430
        squashfs: real=10.2740   sys=5.7360    user=0.0510
        cramfs:   real=7.7970    sys=5.6260    user=0.0430

    Test 4: seq access, stat-only
    Command: `tools/frd -e -s 1387916272 -N -i /home/pi/perf/tmpfs/tmp/all-paths.txt`
        microfs:  real=0.0210    sys=0.0070    user=0.0030
        squashfs: real=0.0200    sys=0.0050    user=0.0050
        cramfs:   real=0.0200    sys=0.0060    user=0.0040

    Test 5: rand access, seq reading
    Command: `tools/frd -e -s 1387916272 -R -i /home/pi/perf/tmpfs/tmp/file-paths.txt`
        microfs:  real=9.0890    sys=6.7960    user=0.0450
        squashfs: real=10.2780   sys=5.7450    user=0.0440
        cramfs:   real=7.8140    sys=5.6560    user=0.0360

    Test 6: rand access, stat-only
    Command: `tools/frd -e -s 1387916272 -R -N -i /home/pi/perf/tmpfs/tmp/all-paths.txt`
        microfs:  real=0.0200    sys=0.0080    user=0.0020
        squashfs: real=0.0200    sys=0.0070    user=0.0030
        cramfs:   real=0.0200    sys=0.0080    user=0.0020

    Test 7: seq access, rand reading
    Command: `tools/frd -e -s 1387916272 -r -i /home/pi/perf/tmpfs/tmp/file-paths.txt`
        microfs:  real=9.0670    sys=6.7890    user=0.0720
        squashfs: real=10.6380   sys=6.0060    user=0.0810
        cramfs:   real=8.7300    sys=6.4380    user=0.0990

    Test 8: rand access, rand reading
    Command: `tools/frd -e -s 1387916272 -R -r -i /home/pi/perf/tmpfs/tmp/file-paths.txt`
        microfs:  real=9.0440    sys=6.7530    user=0.0970
        squashfs: real=10.6460   sys=6.0300    user=0.0690
        cramfs:   real=8.7060    sys=6.4240    user=0.1090

##### Block size 131072 bytes, 10 passes

Command:

    linux-microfs/tools/rofsbench.sh -n 10 -r 1387916272 \
        -b 131072 -B 117440512 -w /home/pi/perf/

Result:

    Test 0: list all recursively
    Command: `ls -lAR /home/pi/perf/tmpfs/mnt`
        microfs:  real=0.2010    sys=0.1040    user=0.0460
        squashfs: real=0.1630    sys=0.0650    user=0.0550
        cramfs:   real=0.1670    sys=0.0560    user=0.0630

    Test 1: find all
    Command: `find /home/pi/perf/tmpfs/mnt`
        microfs:  real=0.0600    sys=0.0200    user=0.0050
        squashfs: real=0.0600    sys=0.0120    user=0.0080
        cramfs:   real=0.0600    sys=0.0100    user=0.0100

    Test 2: find files
    Command: `find /home/pi/perf/tmpfs/mnt -type f`
        microfs:  real=0.0700    sys=0.0190    user=0.0110
        squashfs: real=0.0600    sys=0.0250    user=0.0050
        cramfs:   real=0.0600    sys=0.0170    user=0.0110

    Test 3: seq access, seq reading
    Command: `tools/frd -e -s 1387916272 -i /home/pi/perf/tmpfs/tmp/file-paths.txt`
        microfs:  real=6.0160    sys=4.3010    user=0.0550
        squashfs: real=8.5080    sys=4.5840    user=0.0500
        cramfs:   real=7.7740    sys=5.6070    user=0.0600

    Test 4: seq access, stat-only
    Command: `tools/frd -e -s 1387916272 -N -i /home/pi/perf/tmpfs/tmp/all-paths.txt`
        microfs:  real=0.0200    sys=0.0050    user=0.0050
        squashfs: real=0.0220    sys=0.0110    user=0.0000
        cramfs:   real=0.0200    sys=0.0050    user=0.0050

    Test 5: rand access, seq reading
    Command: `tools/frd -e -s 1387916272 -R -i /home/pi/perf/tmpfs/tmp/file-paths.txt`
        microfs:  real=6.0290    sys=4.3200    user=0.0480
        squashfs: real=8.6120    sys=4.7280    user=0.0360
        cramfs:   real=7.8170    sys=5.6470    user=0.0480

    Test 6: rand access, stat-only
    Command: `tools/frd -e -s 1387916272 -R -N -i /home/pi/perf/tmpfs/tmp/all-paths.txt`
        microfs:  real=0.0200    sys=0.0050    user=0.0050
        squashfs: real=0.0200    sys=0.0070    user=0.0030
        cramfs:   real=0.0200    sys=0.0070    user=0.0030

    Test 7: seq access, rand reading
    Command: `tools/frd -e -s 1387916272 -r -i /home/pi/perf/tmpfs/tmp/file-paths.txt`
        microfs:  real=6.0700    sys=4.3420    user=0.0640
        squashfs: real=8.4540    sys=4.5570    user=0.0540
        cramfs:   real=8.7370    sys=6.4860    user=0.0710

    Test 8: rand access, rand reading
    Command: `tools/frd -e -s 1387916272 -R -r -i /home/pi/perf/tmpfs/tmp/file-paths.txt`
        microfs:  real=6.0660    sys=4.3280    user=0.0720
        squashfs: real=8.6100    sys=4.7060    user=0.0570
        cramfs:   real=8.7230    sys=6.4480    user=0.0950


#### 112 MB; 0 directories, 36 files

##### Block size 4096 bytes, 10 passes

Command:

    linux-microfs/tools/rofsbench.sh -n 10 -r 1389877799 \
        -b 4096 -B 117440512 -w /home/pi/perf/

Result:

    Test 0: list all recursively
    Command: `ls -lAR /home/pi/perf/tmpfs/mnt`
        microfs:  real=0.1010    sys=0.0260    user=0.0150
        squashfs: real=0.1030    sys=0.0270    user=0.0130
        cramfs:   real=0.0940    sys=0.0270    user=0.0130

    Test 1: find all
    Command: `find /home/pi/perf/tmpfs/mnt`
        microfs:  real=0.0520    sys=0.0070    user=0.0030
        squashfs: real=0.0500    sys=0.0080    user=0.0020
        cramfs:   real=0.0520    sys=0.0090    user=0.0020

    Test 2: find files
    Command: `find /home/pi/perf/tmpfs/mnt -type f`
        microfs:  real=0.0500    sys=0.0060    user=0.0050
        squashfs: real=0.0500    sys=0.0090    user=0.0010
        cramfs:   real=0.0500    sys=0.0050    user=0.0050

    Test 3: seq access, seq reading
    Command: `tools/frd -e -s 1389877799 -i /home/pi/perf/tmpfs/tmp/file-paths.txt`
        microfs:  real=9.0470    sys=6.7440    user=0.0490
        squashfs: real=10.6530   sys=6.0670    user=0.0410
        cramfs:   real=7.7680    sys=5.6060    user=0.0440

    Test 4: seq access, stat-only
    Command: `tools/frd -e -s 1389877799 -N -i /home/pi/perf/tmpfs/tmp/all-paths.txt`
        microfs:  real=0.0200    sys=0.0080    user=0.0020
        squashfs: real=0.0210    sys=0.0070    user=0.0030
        cramfs:   real=0.0220    sys=0.0090    user=0.0010

    Test 5: rand access, seq reading
    Command: `tools/frd -e -s 1389877799 -R -i /home/pi/perf/tmpfs/tmp/file-paths.txt`
        microfs:  real=9.0340    sys=6.7370    user=0.0480
        squashfs: real=10.6470   sys=6.0670    user=0.0450
        cramfs:   real=7.7990    sys=5.6060    user=0.0530

    Test 6: rand access, stat-only
    Command: `tools/frd -e -s 1389877799 -R -N -i /home/pi/perf/tmpfs/tmp/all-paths.txt`
        microfs:  real=0.0200    sys=0.0080    user=0.0020
        squashfs: real=0.0200    sys=0.0050    user=0.0050
        cramfs:   real=0.0210    sys=0.0100    user=0.0000

    Test 7: seq access, rand reading
    Command: `tools/frd -e -s 1389877799 -r -i /home/pi/perf/tmpfs/tmp/file-paths.txt`
        microfs:  real=9.0660    sys=6.7780    user=0.0750
        squashfs: real=11.0610   sys=6.3760    user=0.0710
        cramfs:   real=8.7260    sys=6.4560    user=0.0780

    Test 8: rand access, rand reading
    Command: `tools/frd -e -s 1389877799 -R -r -i /home/pi/perf/tmpfs/tmp/file-paths.txt`
        microfs:  real=9.0850    sys=6.7720    user=0.0890
        squashfs: real=11.0750   sys=6.3770    user=0.0790
        cramfs:   real=8.7440    sys=6.4620    user=0.0910

##### Block size 131072 bytes, 10 passes

Command:

    linux-microfs/tools/rofsbench.sh -n 10 -r 1389877799 \
        -b 131072 -B 117440512 -w /home/pi/perf/

Result:

    Test 0: list all recursively
    Command: `ls -lAR /home/pi/perf/tmpfs/mnt`
        microfs:  real=0.0860    sys=0.0240    user=0.0160
        squashfs: real=0.0800    sys=0.0200    user=0.0190
        cramfs:   real=0.0820    sys=0.0280    user=0.0080

    Test 1: find all
    Command: `find /home/pi/perf/tmpfs/mnt`
        microfs:  real=0.0500    sys=0.0050    user=0.0050
        squashfs: real=0.0500    sys=0.0040    user=0.0060
        cramfs:   real=0.0500    sys=0.0040    user=0.0060

    Test 2: find files
    Command: `find /home/pi/perf/tmpfs/mnt -type f`
        microfs:  real=0.0500    sys=0.0030    user=0.0070
        squashfs: real=0.0520    sys=0.0070    user=0.0040
        cramfs:   real=0.0520    sys=0.0060    user=0.0040

    Test 3: seq access, seq reading
    Command: `tools/frd -e -s 1389877799 -i /home/pi/perf/tmpfs/tmp/file-paths.txt`
        microfs:  real=5.9700    sys=4.2700    user=0.0420
        squashfs: real=8.4100    sys=4.5170    user=0.0400
        cramfs:   real=7.7640    sys=5.5820    user=0.0470

    Test 4: seq access, stat-only
    Command: `tools/frd -e -s 1389877799 -N -i /home/pi/perf/tmpfs/tmp/all-paths.txt`
        microfs:  real=0.0100    sys=0.0070    user=0.0030
        squashfs: real=0.0100    sys=0.0050    user=0.0050
        cramfs:   real=0.0110    sys=0.0050    user=0.0050

    Test 5: rand access, seq reading
    Command: `tools/frd -e -s 1389877799 -R -i /home/pi/perf/tmpfs/tmp/file-paths.txt`
        microfs:  real=5.9820    sys=4.2760    user=0.0370
        squashfs: real=8.4210    sys=4.5160    user=0.0470
        cramfs:   real=7.7580    sys=5.5780    user=0.0460

    Test 6: rand access, stat-only
    Command: `tools/frd -e -s 1389877799 -R -N -i /home/pi/perf/tmpfs/tmp/all-paths.txt`
        microfs:  real=0.0100    sys=0.0070    user=0.0030
        squashfs: real=0.0110    sys=0.0100    user=0.0000
        cramfs:   real=0.0110    sys=0.0070    user=0.0030

    Test 7: seq access, rand reading
    Command: `tools/frd -e -s 1389877799 -r -i /home/pi/perf/tmpfs/tmp/file-paths.txt`
        microfs:  real=6.0380    sys=4.2920    user=0.0660
        squashfs: real=8.4040    sys=4.4820    user=0.0620
        cramfs:   real=8.7260    sys=6.4260    user=0.1020

    Test 8: rand access, rand reading
    Command: `tools/frd -e -s 1389877799 -R -r -i /home/pi/perf/tmpfs/tmp/file-paths.txt`
        microfs:  real=6.0140    sys=4.3030    user=0.0500
        squashfs: real=8.3960    sys=4.4840    user=0.0640
        cramfs:   real=8.7240    sys=6.4560    user=0.0790

## A small list of mixed TODOs, FIXMEs, WTFs and the sort

 * The devtable support seems to be quite buggy when tested
   on real machines (non-virtual).
 * Update the benchmark tools.

## Ideas for possible future development

 * Add support for compressed metadata?
 * Implement per-cpu decompression to improve performance for
   parallel I/O?
