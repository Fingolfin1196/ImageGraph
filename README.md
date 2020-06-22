# ImageGraph
An image processing library inspired by libvips with improvements for use in a memory-constrained, time-efficient manner.
There are many interesting components here that I will describe in a paper that I am in the process of preparing.

# License
The library is licensed under the GPL version 3, a copy of which can be found under [gpl-3.0.txt](gpl-3.0.txt).

# Dependencies
* [libvips](https://github.com/libvips/libvips) (licensed under the LGPL version 2.1)
* [Abseil](https://abseil.io/) (licensed under the Apache version 2.0 license)
* [PCG](https://github.com/Fingolfin1196/pcg-cpp) (licensed under the Apache version 2.0 license)
* [Intel Threading Building Blocks](https://software.intel.com/content/www/us/en/develop/tools/threading-building-blocks.html) (licensed under the Apache version 2.0 license)

# Building
libvips and the Intel Threading Building Blocks can be installed as usual.

For PCG, the link above leads to my fork that includes some haphazard CMake support, which makes it possible to install it as usual.

Abseil is linked as a static library into the shared library created by this project.
Creating a static library is only possible for LTS releases and did not work for me without disabling GMock and tests.

More detailed build instructions will follow shortly.

Additionally, the Hilbert spiral generator is based on [gilbert](https://github.com/Fingolfin1196/gilbert), albeit somewhat loosely, which is licensed under the 2-Clause BSD license that can be found under [other-licenses/gilbert-bsd-2-clause.txt](other-licenses/gilbert-bsd-2-clause.txt).
