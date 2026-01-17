# Lyra Engine

**Lyra Engine** is an effort to build a rendering engine from scratch.

NOTE: This project is still a work in progress, some of the features might be defined, but unimplemented.
This is a project develop and maintained by single person. Feedbacks and contributons are welcome, but I
have limited time for this project after work, so please do expect slower response time and development speed.

## Design

For more details in project design and development, please refer to `Devlogs` directory.

* [Overview](Devlogs/Overview.md)
* [RHI Design & Implementation](Devlogs/RHI.md)

## Build

**Lyra-Engine** uses [CMake](https://cmake.org/) with [vckpkg](https://vcpkg.io/en/) to
manage the build system and the 3rd party dependencies. To simplify the command line,
we adopt [just](https://just.systems/) as the primary command invoker for a lot of
the commonly used commands.

Prior to build, user must specify environment variable **VCPKG_ROOT**.
User can specify one of the presets from **CMakePresets.json**.
User can also create **CMakeUserPresets.json** to overwrite the default configuration.

Prior to configure, user could run `just list` to query the available build presets.
For example, this is the output from my development machine.

```
Available configure presets:

  "ninja" - Ninja
  "msvc"  - Visual Studio
  "xcode" - Xcode

Available build presets:

  "msvc-debug"    - Visual Studio Debug Build
  "msvc-release"  - Visual Studio Release Build
  "xcode-debug"   - Xcode Debug Build
  "xcode-release" - Xcode Release Build
  "ninja-debug"   - Ninja Debug Build
  "ninja-release" - Ninja Release Build
```

Selecting from one of the build presets above, user could run one of the following
command to configure cmake. This build reciple contains two parts: generator and build mode.
The generator is from one of the configure presets, and the build mode is either "debug" or "release".

```bash
just config msvc  debug  # use MSVC, recommended on Windows
just config xcode debug  # use Xcode, recommended on MacOS
just config ninja debug  # use Ninja multi-config, recommended on Linux
```

Once the configure command is invoked, vcpkg will automatically install the required
dependencies defined by **vcpkg.json**. Some of the dependencies might take a while
to compile.

The above configure command also caches the user specified preset into `Scratch/config.json`.
This is used for all following `build/test/run` commands. Users can run `config` command again
to update this selected build config.

To build the project, users can use the following command:

```bash
just build              # build everything
just build vulkan       # build specific lyra component (target in `lyra-*` form)
```

## Test

To keep the development process robust, users can build the `testkit` target.
This target is a custom target to launch the test script using Python3. This
script will call the testkit binary in order and compose an HTML page for result.
Currently there is no automatic checking for graphics result, so manual check is
still required.

```bash
just test                   # run all tests and generate report
just test graphics_pipeline # only run tests with "graphics_pipeline" in the test name
```

## Samples

**Lyra-Engine** plans to include both editor and player samples in this repository.
To run the editor/player, user can run the following command:

```bash
just run editor
```

## Install

User can also install the built (release) binary into system directory.
Once installed, other projects can use `find_pacakge(Lyra-Engine)` and
use it from other projects.

```bash
cmake --install Scratch  # copy the built libraries and headers into system directory
```

## Author(s)

[Tianyu Cheng](tianyu.cheng@utexas.edu)
