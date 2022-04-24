
<!-- README.md is generated from README.Rmd. Please edit that file -->

# s2geography

<!-- badges: start -->
<!-- badges: end -->

Google’s [s2geometry](https://github.com/google/s2geometry) is a
spherical geometry engine providing accurate and performant geometry
operations for geometries on the sphere. This library provides a
compatability layer on top of s2geometry for those more familiar with
[simple features](https://en.wikipedia.org/wiki/Simple_Features),
[GEOS](https://libgeos.org), and/or the [GEOS C
API](https://libgeos.org/doxygen/geos__c_8h.html).

The s2geography library was refactored out of the [s2 package for
R](https://github.com/r-spatial/s2), which has served as the backend for
geometries with geographic coordinates in the popular [sf package for
R](https://github.com/r-spatial/sf) since version 1.0.0. The library is
currently under construction as it adapts to suit the needs of more than
just a single R package.

## Usage

s2geography depends on s2geometry, which depends on
[Abseil](https://github.com/abseil/abseil-cpp) and OpenSSL. If you
already have a project that uses s2geometry, you should probably just
copy the contents of the src/ directory into your project and
`#include "s2geography.h"`.

Otherwise you can build and install using `cmake`. The project is
structured such that the VSCode `cmake` integration is triggered when
the folder is open (if the default build doesn’t work, consider adding
`CMakeUserPresets.json` to configure things like the install directory
or the location of OpenSSL). You can also configure manually:

``` bash
git clone https://github.com/paleolimbot/s2geography.git s2geography
mkdir s2geography/build && cd s2geography/build

# you may need to specify the location of OpenSSL
# (e.g., `cmake .. -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@1.1` on M1 mac)
cmake ..
cmake --build .
cmake --install . --prefix ../dist
```

This will also download, build, and install Abseil and s2geometry to the
install directory. By default this will build and install static
libraries for s2geography, s2, and many abseil libraries to the dist/
directory. If you are building your own project with CMake, you should
be able to do:

``` cmake
include(FetchContent)
FetchContent_Declare(
  s2geography
  GIT_REPOSITORY https://github.com/paleolimbot/s2geography.git
  GIT_TAG <PICK A COMMIT SHA1>
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(s2geography)
```

Otherwise, you can add some compier/linker flags and
`#include "s2geography.h"`. This will also let you `#include` any/all of
s2geometry.

    CPPFLAGS = -I<install_prefix>/include -I<openssl_root_dir>/include
    LDFLAGS = -L<openssl_root_dir>/lib -lssl -lcrypto -L<install_prefix>/lib -ls2geography -ls2 -labsl_base -labsl_city -labsl_demangle_internal -labsl_flags_reflection -labsl_graphcycles_internal -labsl_hash -labsl_hashtablez_sampler -labsl_int128 -labsl_low_level_hash -labsl_malloc_internal -labsl_raw_hash_set -labsl_raw_logging_internal -labsl_spinlock_wait -labsl_stacktrace -labsl_str_format_internal -labsl_strings -labsl_symbolize -labsl_synchronization -labsl_throw_delegate -labsl_time_zone -labsl_time

Somebody who is better than me at `cmake` could probably do a better job
making all of this flexible and/or get shared libraries to work.

# Example

A quick example (using R and the [cpp11](https://cpp11.r-lib.org/)
package) (using `Sys.setenv()` to set the `PKG_CXXFLAGS` and `PKG_LIBS`
environment variables to the `CPPFLAGS` and `LDFLAGS`)

``` cpp
#include "cpp11.hpp"
#include "s2geography.h"

using namespace s2geography;

[[cpp11::register]]
void test_s2geography() {
  PointGeography point1 = S2LatLng::FromDegrees(45, -64).ToPoint();
  PointGeography point2 = S2LatLng::FromDegrees(45, 0).ToPoint();

  ShapeIndexGeography point1_index(point1);
  ShapeIndexGeography point2_index(point2);

  double dist = s2_distance(point1_index, point2_index);

  Rprintf("distance result is %g", dist);
}
```

``` r
test_s2geography()
#> distance result is 0.768167
```
