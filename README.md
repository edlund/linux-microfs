
# microfs - Minimally Improved Compressed Read Only File System

https://github.com/edlund/linux-microfs

    microfs is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU General Public License for more details.

microfs is basically a reimplementation of cramfs with a few new
features added which are not backwards compatible. It is mostly
implemented to serve as an exercise for its author and might not
be particularly useful in any real world scenario. If you are
looking for a cramfs replacement, you probably want to consider
squashfs (which is an excellent read only file system). With that
said, you might still find microfs interesting, particularly if
you appreciate cramfs for its awesome simplicity.

The difference between cramfs and microfs lies in that microfs

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
   different mounted images. (microfs will therefore always use
   more memory than cramfs.)
 * Does not support file holes by simply skipping the zero bytes
   like cramfs, instead it will treat file holes as any other data
   and compress it.

## Licensing

Most of microfs is GPL v.2, just like the Linux kernel. There
are however a few utilities which might be useful outside of
the project and are licensed under the new BSD license. Check
the copyright and license notice at the top of the file to know
what license a specific file is under. GPL v.2 will apply if no
license is explicitly specified in a file.

## Linux kernel compatibility

microfs currently targets the latest Ubuntu release (Ubuntu 13.10
"Saucy Salamander", linux 3.11), this is mostly because the
upstream kernel is such a fast moving target that it would
require extra effort to develop against.

## Building microfs

It is presumed that you are planning to build microfs on a
GNU/Linux system which can compile the Linux kernel. Adding
to the requirements implied by this assumption microfs will
also need the following things in order to compile successfully:

 * gcc (>=4.7), http://gcc.gnu.org/
 * GNU make (>=3.81), http://www.gnu.org/software/make/
 * check unit test framework (>=0.9.8), http://check.sourceforge.net/
 * zlib (>=1.2.7), http://www.zlib.net/
 * python (>=2.7), http://www.python.org/
 * perl5 (>=v5.14), http://www.perl.org/
 * cramfs-tools (>=1.1), http://sourceforge.net/projects/cramfs/ (opt)
 * squashfs-tools (>=4.2), http://squashfs.sourceforge.net/ (opt)
 * inotify-tools (>=3.14), https://github.com/rvoicilas/inotify-tools (opt)
 * tmpfs (>=3.11.0), https://www.kernel.org/ (opt)

The easiest way to set up a temporary build environment to try
microfs is to install Ubuntu on a virtual machine and use
`tools/packman.sh` with an appropriate package file.

    $ tools/packman.sh apt extras/packagelists/ubuntu-13.10.txt

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
testing of microfs. Most of the data used for the tests is
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

microfs supports "remote checks", which makes it easy to run
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
performance. microfs is shipped with a couple of simple benchmark
tools which can be used to compare the performance of `microfs`,
`cramfs` and `squashfs` for common filesystem operations.

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
`cramfs` will still use `PAGE_CACHE_SIZE` sized blocks and
will not keep up.

The result presented by the benchmark tools is the average
time that a test took over `N` passes. The idea is that the
average should give a more fair representation of the performance
of the filesystem than a single pass would.

Still, with all this said, it is still probably wise to consider
the bundled benchmark tools to be biased towards making microfs
look good, even if that is not the intention.

### Results

A few results from benchmarks performed on a test machine
is included. They are listed below. The command found before
each result listing can be used to run the benchmark that
generated the result in question.

The test machine is a VirtualBox VM running Ubuntu 13.10
x86-64 with 1280MB RAM and 2 virtual processors.

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
        microfs:   real=0.1610    sys=0.0070    user=0.0000
        squashfs:  real=0.2280    sys=0.0160    user=0.0030
        cramfs:    real=0.2120    sys=0.0110    user=0.0000

    Test 1: find all
    Command: `find /tmp/perf/tmpfs/mnt`
        microfs:   real=0.0460    sys=0.0000    user=0.0000
        squashfs:  real=0.0530    sys=0.0000    user=0.0000
        cramfs:    real=0.0760    sys=0.0010    user=0.0000

    Test 2: find files
    Command: `find /tmp/perf/tmpfs/mnt -type f`
        microfs:   real=0.0480    sys=0.0000    user=0.0000
        squashfs:  real=0.0510    sys=0.0000    user=0.0000
        cramfs:    real=0.0590    sys=0.0000    user=0.0000

    Test 3: seq access, seq reading
    Command: `tools/frd -e -s 1387916272 -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=12.4210    sys=9.2630    user=0.0010
        squashfs:  real=11.8230    sys=9.0710    user=0.0010
        cramfs:    real=11.8710    sys=8.8330    user=0.0000

    Test 4: seq access, stat-only
    Command: `tools/frd -e -s 1387916272 -N -i /tmp/perf/all-paths.txt`
        microfs:   real=0.0100    sys=0.0000    user=0.0000
        squashfs:  real=0.0090    sys=0.0000    user=0.0000
        cramfs:    real=0.0080    sys=0.0000    user=0.0000

    Test 5: rand access, seq reading
    Command: `tools/frd -e -s 1387916272 -R -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=11.1430    sys=8.3980    user=0.0020
        squashfs:  real=11.5820    sys=8.9140    user=0.0040
        cramfs:    real=11.1760    sys=8.4140    user=0.0000

    Test 6: rand access, stat-only
    Command: `tools/frd -e -s 1387916272 -R -N -i /tmp/perf/tmpfs/tmp/all-paths.txt`
        microfs:   real=0.0090    sys=0.0000    user=0.0000
        squashfs:  real=0.0090    sys=0.0000    user=0.0000
        cramfs:    real=0.0110    sys=0.0000    user=0.0000

    Test 7: seq access, rand reading
    Command: `tools/frd -e -s 1387916272 -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=10.0010    sys=7.6200    user=0.0010
        squashfs:  real=11.4620    sys=8.8530    user=0.0080
        cramfs:    real=11.8700    sys=8.8760    user=0.0040

    Test 8: rand access, rand reading
    Command: `tools/frd -e -s 1387916272 -R -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=10.1940    sys=7.7250    user=0.0060
        squashfs:  real=11.4940    sys=8.9160    user=0.0020
        cramfs:    real=11.6400    sys=8.7620    user=0.0050

##### Block size 131072 bytes, 10 passes

Command:
    $ tools/rofsbench.sh -n 10 -r 1387916272 -b 131072 -w /tmp/perf/
Result:
    Test 0: list all recursively
    Command: `ls -lAR /tmp/perf/tmpfs/mnt`
        microfs:   real=0.1670    sys=0.0060    user=0.0010
        squashfs:  real=0.1690    sys=0.0110    user=0.0000
        cramfs:    real=0.1790    sys=0.0070    user=0.0010

    Test 1: find all
    Command: `find /tmp/perf/tmpfs/mnt`
        microfs:   real=0.0460    sys=0.0000    user=0.0000
        squashfs:  real=0.0460    sys=0.0000    user=0.0000
        cramfs:    real=0.0490    sys=0.0000    user=0.0000

    Test 2: find files
    Command: `find /tmp/perf/tmpfs/mnt -type f`
        microfs:   real=0.0460    sys=0.0000    user=0.0000
        squashfs:  real=0.0470    sys=0.0000    user=0.0000
        cramfs:    real=0.0480    sys=0.0000    user=0.0000

    Test 3: seq access, seq reading
    Command: `tools/frd -e -s 1387916272 -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.9980    sys=0.8560    user=0.0020
        squashfs:  real=1.0360    sys=0.9400    user=0.0000
        cramfs:    real=10.8740   sys=8.2170    user=0.0010

    Test 4: seq access, stat-only
    Command: `tools/frd -e -s 1387916272 -N -i /tmp/perf/tmpfs/tmp/all-paths.txt`
        microfs:   real=0.0080    sys=0.0000    user=0.0000
        squashfs:  real=0.0090    sys=0.0000    user=0.0000
        cramfs:    real=0.0090    sys=0.0000    user=0.0000

    Test 5: rand access, seq reading
    Command: `tools/frd -e -s 1387916272 -R -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.9980    sys=0.8610    user=0.0000
        squashfs:  real=1.0530    sys=0.9590    user=0.0010
        cramfs:    real=10.7760   sys=8.1140    user=0.0010

    Test 6: rand access, stat-only
    Command: `tools/frd -e -s 1387916272 -R -N -i /tmp/perf/tmpfs/tmp/all-paths.txt`
        microfs:   real=0.0110    sys=0.0000    user=0.0000
        squashfs:  real=0.0100    sys=0.0000    user=0.0000
        cramfs:    real=0.0090    sys=0.0000    user=0.0000

    Test 7: seq access, rand reading
    Command: `tools/frd -e -s 1387916272 -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=1.0410    sys=0.9030    user=0.0000
        squashfs:  real=1.0900    sys=0.9760    user=0.0000
        cramfs:    real=11.1880   sys=8.4960    user=0.0070

    Test 8: rand access, rand reading
    Command: `tools/frd -e -s 1387916272 -R -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=1.0210    sys=0.8880    user=0.0020
        squashfs:  real=1.0820    sys=0.9790    user=0.0010
        cramfs:    real=10.9930   sys=8.3330    user=0.0040

#### 0 directories, 46 files, large files

##### Block size 4096 bytes, 10 passes

Command:
    $ tools/rofsbench.sh -n 10 -r 1389877799 -b 4096 -w /tmp/perf/
Result:
    Test 0: list all recursively
    Command: `ls -lAR /tmp/perf/tmpfs/mnt`
        microfs:   real=0.2080    sys=0.0070    user=0.0000
        squashfs:  real=0.2160    sys=0.0130    user=0.0000
        cramfs:    real=0.2260    sys=0.0070    user=0.0000

    Test 1: find all
    Command: `find /tmp/perf/tmpfs/mnt`
        microfs:   real=0.0580    sys=0.0000    user=0.0000
        squashfs:  real=0.0580    sys=0.0000    user=0.0000
        cramfs:    real=0.0530    sys=0.0010    user=0.0000

    Test 2: find files
    Command: `find /tmp/perf/tmpfs/mnt -type f`
        microfs:   real=0.0540    sys=0.0000    user=0.0000
        squashfs:  real=0.0460    sys=0.0000    user=0.0000
        cramfs:    real=0.0500    sys=0.0000    user=0.0000

    Test 3: seq access, seq reading
    Command: `tools/frd -e -s 1389877799 -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=12.0250    sys=8.9840    user=0.0010
        squashfs:  real=11.1680    sys=8.6620    user=0.0010
        cramfs:    real=11.4420    sys=8.5580    user=0.0010

    Test 4: seq access, stat-only
    Command: `tools/frd -e -s 1389877799 -N -i /tmp/perf/all-paths.txt`
        microfs:   real=0.0120    sys=0.0000    user=0.0000
        squashfs:  real=0.0070    sys=0.0000    user=0.0000
        cramfs:    real=0.0090    sys=0.0000    user=0.0000

    Test 5: rand access, seq reading
    Command: `tools/frd -e -s 1389877799 -R -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=10.8540    sys=8.2530    user=0.0010
        squashfs:  real=10.8580    sys=8.4890    user=0.0020
        cramfs:    real=10.6540    sys=8.0950    user=0.0010

    Test 6: rand access, stat-only
    Command: `tools/frd -e -s 1389877799 -R -N -i /tmp/perf/tmpfs/tmp/all-paths.txt`
        microfs:   real=0.0070    sys=0.0000    user=0.0000
        squashfs:  real=0.0060    sys=0.0000    user=0.0000
        cramfs:    real=0.0090    sys=0.0000    user=0.0000

    Test 7: seq access, rand reading
    Command: `tools/frd -e -s 1389877799 -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=9.2140     sys=7.1060    user=0.0030
        squashfs:  real=10.8770    sys=8.5130    user=0.0050
        cramfs:    real=10.8930    sys=8.2640    user=0.0090

    Test 8: rand access, rand reading
    Command: `tools/frd -e -s 1389877799 -R -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=10.3010    sys=7.7720    user=0.0030
        squashfs:  real=10.8150    sys=8.4990    user=0.0020
        cramfs:    real=10.7940    sys=8.1850    user=0.0040

##### Block size 131072 bytes, 10 passes

Command:
    $ tools/rofsbench.sh -n 10 -r 1389877799 -b 131072 -w /tmp/perf/
Result:
    Test 0: list all recursively
    Command: `ls -lAR /tmp/perf/tmpfs/mnt`
        microfs:   real=0.1690    sys=0.0040    user=0.0000
        squashfs:  real=0.1670    sys=0.0060    user=0.0000
        cramfs:    real=0.1820    sys=0.0040    user=0.0000

    Test 1: find all
    Command: `find /tmp/perf/tmpfs/mnt`
        microfs:   real=0.0450    sys=0.0000    user=0.0000
        squashfs:  real=0.0540    sys=0.0000    user=0.0000
        cramfs:    real=0.0640    sys=0.0000    user=0.0000

    Test 2: find files
    Command: `find /tmp/perf/tmpfs/mnt -type f`
        microfs:   real=0.0440    sys=0.0000    user=0.0000
        squashfs:  real=0.0500    sys=0.0000    user=0.0000
        cramfs:    real=0.0520    sys=0.0000    user=0.0000

    Test 3: seq access, seq reading
    Command: `tools/frd -e -s 1389877799 -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.9330    sys=0.8140    user=0.0000
        squashfs:  real=1.0260    sys=0.9280    user=0.0000
        cramfs:    real=11.2580   sys=8.4820    user=0.0010

    Test 4: seq access, stat-only
    Command: `tools/frd -e -s 1389877799 -N -i /tmp/perf/tmpfs/tmp/all-paths.txt`
        microfs:   real=0.0090    sys=0.0000    user=0.0000
        squashfs:  real=0.0060    sys=0.0000    user=0.0000
        cramfs:    real=0.0080    sys=0.0000    user=0.0000

    Test 5: rand access, seq reading
    Command: `tools/frd -e -s 1389877799 -R -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.9430    sys=0.8150    user=0.0000
        squashfs:  real=0.9870    sys=0.9040    user=0.0000
        cramfs:    real=11.0770   sys=8.3710    user=0.0020

    Test 6: rand access, stat-only
    Command: `tools/frd -e -s 1389877799 -R -N -i /tmp/perf/all-paths.txt`
        microfs:   real=0.0080    sys=0.0000    user=0.0000
        squashfs:  real=0.0120    sys=0.0000    user=0.0000
        cramfs:    real=0.0080    sys=0.0000    user=0.0000

    Test 7: seq access, rand reading
    Command: `tools/frd -e -s 1389877799 -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.9800    sys=0.8560    user=0.0020
        squashfs:  real=1.0950    sys=0.9580    user=0.0010
        cramfs:    real=11.3550   sys=8.6180    user=0.0030

    Test 8: rand access, rand reading
    Command: `tools/frd -e -s 1389877799 -R -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=1.0070    sys=0.8750    user=0.0010
        squashfs:  real=1.0330    sys=0.9260    user=0.0020
        cramfs:    real=11.4580   sys=8.6770    user=0.0060

### Official results from the ministry of thruth

Benchmarks tweaked to make microfs look a little better.

#### 19 directories, 337 files, small files

##### Block size 1048576 bytes, 10 passes, data compressed for speed

Command:
	$ tools/rofsbench.sh -n 10 -r 1387916272 -b 1048576 \
	>     -m "-c speed" -w /tmp/perf/
Result:
    Test 0: list all recursively
    Command: `ls -lAR /tmp/perf/tmpfs/mnt`
        microfs:   real=0.1980    sys=0.0060    user=0.0020
        squashfs:  real=0.1950    sys=0.0080    user=0.0020
        cramfs:    real=0.2680    sys=0.0090    user=0.0040

    Test 1: find all
    Command: `find /tmp/perf/tmpfs/mnt`
        microfs:   real=0.0580    sys=0.0000    user=0.0000
        squashfs:  real=0.0800    sys=0.0000    user=0.0000
        cramfs:    real=0.0580    sys=0.0010    user=0.0000

    Test 2: find files
    Command: `find /tmp/perf/tmpfs/mnt -type f`
        microfs:   real=0.0490    sys=0.0000    user=0.0000
        squashfs:  real=0.0450    sys=0.0000    user=0.0000
        cramfs:    real=0.0470    sys=0.0000    user=0.0000

    Test 3: seq access, seq reading
    Command: `tools/frd -e -s 1387916272 -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.7910    sys=0.6730    user=0.0000
        squashfs:  real=0.9060    sys=0.7620    user=0.0000
        cramfs:    real=11.2860   sys=8.5870    user=0.0000

    Test 4: seq access, stat-only
    Command: `tools/frd -e -s 1387916272 -N -i /tmp/perf/tmpfs/tmp/all-paths.txt`
        microfs:   real=0.0120    sys=0.0000    user=0.0000
        squashfs:  real=0.0070    sys=0.0000    user=0.0000
        cramfs:    real=0.0110    sys=0.0000    user=0.0000

    Test 5: rand access, seq reading
    Command: `tools/frd -e -s 1387916272 -R -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.8170    sys=0.7020    user=0.0000
        squashfs:  real=0.9930    sys=0.8600    user=0.0030
        cramfs:    real=10.9090   sys=8.3470    user=0.0000

    Test 6: rand access, stat-only
    Command: `tools/frd -e -s 1387916272 -R -N -i /tmp/perf/tmpfs/tmp/all-paths.txt`
        microfs:   real=0.0100    sys=0.0000    user=0.0000
        squashfs:  real=0.0120    sys=0.0000    user=0.0000
        cramfs:    real=0.0100    sys=0.0000    user=0.0000

    Test 7: seq access, rand reading
    Command: `tools/frd -e -s 1387916272 -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.8250    sys=0.7070    user=0.0020
        squashfs:  real=0.8900    sys=0.7510    user=0.0040
        cramfs:    real=11.4610   sys=8.7920    user=0.0060

    Test 8: rand access, rand reading
    Command: `tools/frd -e -s 1387916272 -R -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.8030    sys=0.6990    user=0.0010
        squashfs:  real=1.0430    sys=0.9030    user=0.0070
        cramfs:    real=11.3170   sys=8.6630    user=0.0040

#### 0 directories, 46 files, large files

##### Block size 1048576 bytes, 10 passes, data compressed for speed

Command:
    $ tools/rofsbench.sh -n 10 -r 1389877799 -b 1048576 \
	>     -m "-c speed" -w /tmp/perf/
Result:
    Test 0: list all recursively
    Command: `ls -lAR /tmp/perf/tmpfs/mnt`
        microfs:   real=0.1900    sys=0.0070    user=0.0000
        squashfs:  real=0.1840    sys=0.0080    user=0.0000
        cramfs:    real=0.2190    sys=0.0030    user=0.0000

    Test 1: find all
    Command: `find /tmp/perf/tmpfs/mnt`
        microfs:   real=0.0440    sys=0.0000    user=0.0000
        squashfs:  real=0.0530    sys=0.0000    user=0.0000
        cramfs:    real=0.0650    sys=0.0000    user=0.0000

    Test 2: find files
    Command: `find /tmp/perf/tmpfs/mnt -type f`
        microfs:   real=0.0460    sys=0.0000    user=0.0010
        squashfs:  real=0.0440    sys=0.0000    user=0.0000
        cramfs:    real=0.0470    sys=0.0000    user=0.0000

    Test 3: seq access, seq reading
    Command: `tools/frd -e -s 1389877799 -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.7630    sys=0.6570    user=0.0010
        squashfs:  real=0.8860    sys=0.7390    user=0.0010
        cramfs:    real=10.9240   sys=8.3200    user=0.0000

    Test 4: seq access, stat-only
    Command: `tools/frd -e -s 1389877799 -N -i /tmp/perf/tmpfs/tmp/all-paths.txt`
        microfs:   real=0.0100    sys=0.0000    user=0.0000
        squashfs:  real=0.0100    sys=0.0000    user=0.0000
        cramfs:    real=0.0070    sys=0.0000    user=0.0000

    Test 5: rand access, seq reading
    Command: `tools/frd -e -s 1389877799 -R -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.7440    sys=0.6430    user=0.0020
        squashfs:  real=0.8720    sys=0.7210    user=0.0020
        cramfs:    real=10.6410   sys=8.1390    user=0.0020

    Test 6: rand access, stat-only
    Command: `tools/frd -e -s 1389877799 -R -N -i /tmp/perf/tmpfs/tmp/all-paths.txt`
        microfs:   real=0.0050    sys=0.0000    user=0.0000
        squashfs:  real=0.0100    sys=0.0000    user=0.0000
        cramfs:    real=0.0120    sys=0.0000    user=0.0000

    Test 7: seq access, rand reading
    Command: `tools/frd -e -s 1389877799 -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.7730    sys=0.6680    user=0.0030
        squashfs:  real=0.9240    sys=0.7510    user=0.0030
        cramfs:    real=11.1020   sys=8.5530    user=0.0010

    Test 8: rand access, rand reading
    Command: `tools/frd -e -s 1389877799 -R -r -i /tmp/perf/tmpfs/tmp/file-paths.txt`
        microfs:   real=0.8120    sys=0.6890    user=0.0040
        squashfs:  real=0.9200    sys=0.7590    user=0.0030
        cramfs:    real=10.6140   sys=8.1740    user=0.0060

## A small list of mixed TODOs, FIXMEs, WTFs and the sort.

 * Complete the most important parts of README.md.
 * The pseudorandomness is not reproducible enough?
 * Add support for `mkrandtree.py` to generate "less random"
   data which will compress acceptably well.
 * The `lkm` support mount options, but they have not actually
   been tested. It should work! But I would not bet money
   on it...
 * Directories are structured and read just like they are in
   cramfs. This is probably a good thing, but no thought or
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
 * Fix `make install`, write `make uninstall`.

## Ideas for possible future development

 * Implement support for different compression algorithms?
 * Add support for compressed metadata?
 * Implement per-cpu decompression to improve performance for
   parallel I/O?
 * Test the "As easy as Pi"-idea (that Raspberry Pi is a good
   unit to test/benchmark changes against)?
