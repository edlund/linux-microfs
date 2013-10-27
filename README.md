
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
 * Support configurable block sizes (ranging from 512 up to 32768
   bytes).
 * Support image sizes larger than 272 mb (the upper limit is
   2^32 bytes).
 * Support slightly longer file names (3 extra bytes).
 * Support ctime/mtime values for inodes (although all files will
   share the same value which is when the image was created).
 * Support different compression settings for zlib when the image
   is created.
 * Support configurable read buffer sizes (per mounted image).
   (microfs will therefore always use more memory than cramfs when
   more than one image is mounted since each image gets its own
   read buffers and zlib stream.)

## Licensing

Most of microfs is GPL v.2, just like the Linux kernel. There are
however a few utilities which might be useful outside of the project
and are licensed under the new BSD license. Check the copyright and
license notice at the top of the file to know what license a specific
file is under. GPL v.2 will apply if no license is explicitly specified
in a file.

## Linux kernel compatibility

microfs should work well with the latest Ubuntu release (currently
Ubuntu 13.10 "Saucy Salamander", linux 3.11.0), this is mostly because
the upstream kernel is such a fast moving target that it requires
extra effort to develop against.

## Building microfs

It is presumed that you are planning to build microfs on a GNU/Linux
system which can compile the Linux kernel. Adding to the requirements
implied by this assumption microfs will also need the following
things in order to compile successfully:

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

Once the build environment is set up it is sufficient to run

    $ make

from the root source directory in order to build the lkm and the
hostprogs.

Use the make command line argument `DEBUG=1` or the combination
`DEBUG=1 DEBUG_SPAM=1` to do a debug build. Please note that a `debug
spam` lkm will spam your syslog with messages (hence the name),
it can however be very useful when dealing with a bug which is
reproducible with a small set of files (or few operations).

## Testing microfs

TODO.

## Here be dragons (or bugs... probably mostly bugs)

A small list of mixed TODOs, FIXMEs, WTFs and the sort.

 * Complete the most important parts of README.md.
 * The `lkm` support mount options to control the read buffer
   sizes, but it has not actually been tested. It should work! But
   I would not bet money on it...
 * Directories are structured and read just like they are in cramfs.
   This is probably a good thing, but no thought or effort at all
   has been put into investigating if that is true (so far).
 * `microfsmki` and `microfscki` both require high amount of RAM
   available since they both use mmap() to map the whole image
   they are working on (the reason being; it is a solution which
   is easy to implement and easy to understand). This is probably
   not an issue since most workstations have plenty of RAM. It
   is however possible that the memory requirement is unreasonable,
   if so, it should be possible to rewrite them.
 * Fix `make install`, write `make uninstall`.

## Ideas for possible future development

 * Implement support for different compression algorithms.
 * Add support for compressed metadata.
 * Investigate if there are better ways to handle caches.
 * Write a simple benchmark tool which can compare the performance
   of cramfs and microfs.
 * Test the "As easy as Pi"-idea (that Raspberry Pi is a good
   unit to test/benchmark changes against).
