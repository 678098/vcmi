# Compiling VCMI

-   Current baseline requirement for building is Ubuntu 20.04
-   Supported C++ compilers for UNIX-like systems are GCC 5.5+ and Clang 13+

Older distributions and compilers might work, but they aren't tested by [Travis CI](https://travis-ci.org/vcmi/vcmi/)

# Installing dependencies

## Prerequisites

To compile, the following packages (and their development counterparts) are needed to build:

-   CMake 3.10 or newer
-   SDL2 with devel packages: mixer, image, ttf
-   zlib and zlib-devel
-   Recommended, if you want to build launcher or map editor: Qt 5, widget and network modules
-   Recommended, FFmpeg libraries, if you want to watch in-game videos: libavformat and libswscale. Their name could be libavformat-devel and libswscale-devel, or ffmpeg-libs-devel or similar names. 
-   Boost C++ libraries v1.48+: program-options, filesystem, system, thread, locale
-   Optional, if you want to build scripting modules: LuaJIT

## On Debian-based systems (e.g. Ubuntu)

For Ubuntu 22.04 (jammy) or newer you need to install this list of packages, including **libtbb2-dev**:

`sudo apt-get install cmake g++ libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev zlib1g-dev libavformat-dev libswscale-dev libboost-dev libboost-filesystem-dev libboost-system-dev libboost-thread-dev libboost-program-options-dev libboost-locale-dev qtbase5-dev libtbb2-dev libluajit-5.1-dev qttools5-dev`

For Ubuntu 21.10 (impish) or older and all versions of Debian (9 stretch - 11 bullseye) you need to install this list of packages, including **libtbb-dev**:

`sudo apt-get install cmake g++ libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev zlib1g-dev libavformat-dev libswscale-dev libboost-dev libboost-filesystem-dev libboost-system-dev libboost-thread-dev libboost-program-options-dev libboost-locale-dev qtbase5-dev libtbb-dev libluajit-5.1-dev qttools5-dev`

Alternatively if you have VCMI installed from repository or PPA you can use:

    sudo apt-get build-dep vcmi

## On RPM-based distributions (e.g. Fedora)

    sudo yum install cmake gcc-c++ SDL2-devel SDL2_image-devel SDL2_ttf-devel SDL2_mixer-devel boost boost-devel boost-filesystem boost-system boost-thread boost-program-options boost-locale zlib-devel ffmpeg-devel ffmpeg-libs qt5-qtbase-devel tbb-devel luajit-devel fuzzylite-devel

## On Arch-based distributions

On Arch-based distributions, there is a development package available for VCMI on the AUR.

It can be found at: <https://aur.archlinux.org/packages/vcmi-git/>

Information about building packages from the Arch User Repository (AUR) can be found at the Arch wiki.

# Getting the sources

VCMI is still in development. We recommend the following initial directory structure:

    .
    ├── vcmi -> contains sources and is under git control
    └── build -> contains build output, makefiles, object files,...

You can get latest sources with:

`git clone -b develop --recursive https://github.com/vcmi/vcmi.git`

# Compilation

## Configuring Makefiles

`mkdir build && cd build`
`cmake ../vcmi`

# Additional options that you may want to use:

## To enable debugging:
`cmake ../vcmi -DCMAKE_BUILD_TYPE=Debug`

**Notice**: The ../vcmi/ is not a typo, it will place makefile scripts into the build dir as the build dir is your working dir when calling CMake.

## Trigger build

`cmake --build . -- -j2`
(-j2 = compile with 2 threads, you can specify any value)

That will generate vcmiclient, vcmiserver, vcmilauncher as well as .so libraries in **build/bin/** directory.

# Package building

## RPM package

The first step is to prepare a RPM build environment. On Fedora systems you can follow this guide: <http://fedoraproject.org/wiki/How_to_create_an_RPM_package#SPEC_file_overview>

1. Download the file rpm/vcmi.spec from any tagged VCMI release for which you wish to build a RPM package via the SVN Browser trac at this URL for example(which is for VCMI 0.9): <http://sourceforge.net/apps/trac/vcmi/browser/tags/0.9/rpm/vcmi.spec>

2. Copy the file to ~/rpmbuild/SPECS

3. Follow instructions in the vcmi.spec. You have to export the corresponding SVN tag, compress it to a g-zipped archive and copy it to ~/rpmbuild/SOURCES. Instructions are written as comments and you can copy/paste commands into terminal.

4. Go to ~/rpmbuild/SPECS and open terminal in this folder and type: 
`rpmbuild -ba vcmi.spec (this will build rpm and source rpm)`

5. Generated RPM is in folder ~/rpmbuild/RPMS

If you want to package the generated RPM above for different processor architectures and operating systems you can use the tool mock. Moreover, it is necessary to install mock-rpmfusion_free due to the packages ffmpeg-devel and ffmpeg-libs which aren't available in the standard RPM repositories(at least for Fedora). Go to ~/rpmbuild/SRPMS in terminal and type:

`mock -r fedora-17-i386-rpmfusion_free path_to_source_RPM`
`mock -r fedora-17-x86_64-rpmfusion_free path_to_source_RPM`

Available root environments and their names are listed in /etc/mock.

## Debian/Ubuntu

1. Install debhelper and devscripts packages

2. Run dpkg-buildpackage command from vcmi source directory

```
sudo apt-get install debhelper devscripts
cd /path/to/source
dpkg-buildpackage
```

To generate packages for different architectures see "-a" flag of dpkg-buildpackage command
