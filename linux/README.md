About
-----

"build-nzbget" is a bash script which is used to build universal installer
for Linux and installer for FreeBSD. The script compiles NZBGet for each
supported platform and CPU-architecture and then packs all produced files
into two installer packages: one for Linux and another for FreeBSD. In
addition the packages with debug binaries are created too.

Prerequisites
-------------

To use the script you need a Linux (virtual) machine.

Script `build-nzbget` supports two kind of toolchains:
   - toolchain from uClibc's Buildroot-project.
   - cross-compiling toolchain built with GCC.


Building toolchains
-------------------

1.  Create directory where you want to keep your files to compile NZBGet.
    > **Note:** Choose the path wisely because it cannot be changed later 
    > without rebuilding all toolchains again.

2.  Put the build script `build-nzbget` into that directory.

3.  Create subdirectories:

    | Directory   | Purpose                                |
    |-------------|----------------------------------------|
    | toolchain   | for toolchains                         |
    | nzbget      | for source code of NZBGet              |
    | output      | to store the results of build script   |
    | setup       | for extra files required for installer |

    4.  Build buildroot-toolchain for one CPU-architecture:

        1.  Download Buildroot distribution archive `buildroot-2015.08.01` from
            http://buildroot.uclibc.org/download.html
   
            If you choose a different version, the instructions may not apply exactly.
        2.  Unpack the tarball into `toolchain` directory.

    3.  Rename the buildroot-directory according to the target CPU-architecture name,
        for example `x86_64` or `armhf`.
        > **Note:** Be careful here. Once the toolchain has been built, the
          directory cannot be renamed or moved. If the path changes, you will
          have to rebuild the toolchain.
    4.  Run `make menuconfig`
    5.  Configure toolchain:
        - Target architecture:
            - choose your target architecture.
        - Build options:
            - libraries (both static and shared).
        - Toolchain:
            - Kernel Headers (Manually specified Linux version).
            - (2.6.32) linux version.
            - Custom kernel headers series (2.6.x).
            - C library: glibc (for x86_64 and armhf) or uClibc (for all other architectures).
            - uClibc library version (uClibc-ng).
            - Enable large file (files > 2 GB) support.
            - Enable IPv6 support.
            - Enable toolchain locale/i18n support.
            - GCC compiler Version (gcc 5.x).
            - Enable C++ support.
            - Build cross gdb for the host.
        - Target packages:

           | Section                       | Library |
           |-------------------------------|---------|
           | Compression and decompression | zlib    |
           | Crypto                        | openssl |
           | JSON/XML                      | libxml2 |
           | Text and terminal handling    | ncurses |
        
        - Save configuration and exit.
         
    6.  Make a few extra manual adjustments:
        - in `packages/ncurses/ncurses.mk` add extra configure parameters to
          option `NCURSES_CONF_OPTS` (with quotation marks):
          `"--with-fallbacks=xterm xterm-color xterm-256color xterm-16color linux vt100 vt200"`
        - in `packages/openssl/openssl.mk` replace `zlib-dynamic` with `zlib`
        - in `packages/glibc/glibc.mk` add extra configure parameter
          `--enable-static-nss`.
        - in directory `packages/glibc/2.20` create new file with name `allocatestack-pax.patch`
          with the following content; the patch fixes incompatibility with hardened kernels:

        **allocatestack-pax.patch**:
        ```
        --- a/nptl/allocatestack.c	2014-09-07 10:09:09.000000000 +0200
        +++ b/nptl/allocatestack.c	2017-11-02 18:02:47.165016000 +0100
        @@ -78,6 +78,7 @@
        #ifndef STACK_ALIGN
        # define STACK_ALIGN __alignof__ (long double)
        #endif
        +#define PF_X 0
          
        /* Default value for minimal stack size after allocating thread
           descriptor and guard.  */
        ```
          
    7.  Run `make` to build the toolchain. It may take an hour or less depending
        on your hardware.
    8.  If C library `glibc` was chosen extra steps are needed to build proper
        static libraries of OpenSSL in addition to already built dynamic
        libraries (which are also required):
        - delete directory `output/build/openssl-x.x.x`.
        - in `packages/openssl/openssl.mk` replace 
          `$(if $(BR2_STATIC_LIBS),no-shared,shared)` with
          `$(if $(BR2_STATIC_LIBS),no-shared,no-shared)`
          and replace `$(if $(BR2_STATIC_LIBS),no-dso,dso)` with
          `$(if $(BR2_STATIC_LIBS),no-dso,no-dso)`.
        - from root directory of toolchain run `make` again.
        - openssl will be rebuilt without shared libraries support.
        - installation step of openssl fails with message:
          `chmod: cannot access ‘../lib/engines/lib*.so’: No such file or directory`.
          ignore the error, static libraries of openssl are already installed
          and will be used by NZBGet.

5.  Now you should have a working toolchain for one CPU-architecture, let's
    test it.

    1.  Change to the ROOTBUILD-directory and run the build script:
    
        `./build-nzbget release bin` _<CPU-Architecture>_
    
    2.  The script creates subdirectory `nzbget/trunk` and checkouts the source
        code of NZBGet from subversion repository
    3.  Then the source code is compiled for chosen CPU-architecture.
    4.  After the compiling a distribution binary archive for the chosen
        CPU-architecture is put into output-directory.

6.  Build unrar and 7za for the CPU-architecture:
    1.  Download source of unrar; Compile for target, either manually or
        using script `build-unpack`.
    2.  Put the compiled binaries of unrar and 7za into setup-directory, add
        suffix `-arch` to unrar and 7za names, for example `unrar-armel`.
    3.  Copy license-files from unrar and 7-Zip projects using names
        `license-unrar.txt` and `license-7zip.txt`.

7.  Now you can build installer for that one CPU-architecture:
    1.  If you build for CPU-architecture which is not supported by NZBGet's
        universal installer you have to edit the script `build-nzbget` and
        add the architecture name into variable `ALLTARGETS`.
    2.  Run the build script with:
  
        `./build-nzbget release installer` _<CPU-Architecture>_
  
    3.  The created installer supports only one CPU-Architecture.
    4.  Run the installer on the target machine (with target CPU-Architecture).

8.  Repeat step for each CPU-Architecture you intend to build the installer for.

9.  To build installer for FreeBSD we need a cross-compiling toolhcain:
    1.  Create directory `x86_64-bsd` inside your toolchain-directory.
    2.  Copy file `build-toolchain-freebsd` into that directory.
    3.  Execute the script build script:

        ``` ./build-toolchain-freebsd```
         
        The script downloads and build the full cross-compiling toolchain.

    4.  To test the FreeBSD toolchain start `build-nzbget` with following parameters:

        ```./build-nzbget x86_64-bsd release```

10. To build installer for all CPU-Architectures listed in variable `ALLTARGETS`
    of the script run the script without choosing CPU-Architecture:

    ```./build-nzbget release```

11. When the script is run without any parameters:
    1.  NZBGet is compiled twice for each CPU-Architecture listed in
        `ALLTARGETS`: once in release mode and once in debug mode.
    2.  Four installers are built: one for release and another for debug for
        each supported platform - Linux and FreeBSD.

Special functions
-----------------
By default the script builds from HEAD of develop branch. To specify another
tag or branch pass it to the script, for example to build a tagged version 14.2:

```./build-nzbget release bin tags/v16.0```

Installers can be built only for version 15.0 and newer. Distribution archives
can be built for older versions too.

To build on certain history point pass commit SHA to the script, for example

```./build-nzbget release bin 13f5ab738880671a8d8381ef0fabc625e269eeef```

To cleanup the output directory before building pass parameter `cleanup`:

```./build-nzbget release cleanup```

To improve compilation speed activate pre-compiled headers and multicore make:

```./build-nzbget release pch core4```
