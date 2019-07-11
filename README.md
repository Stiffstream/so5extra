[TOC]

# What Is It?

so_5_extra is a collection of various SObjectizer's extensions. so_5_extra is built on top of SObjectizer and intended to simplify development of SObjectizer-based applications.

At the current moment so_5_extra contains the following components:

* so_5::extra::async_op. Several implementation of *async operations*. Contains subcomponents so_5::extra::async_op::time_unlimited (async operations without a limit for execution time) and so_5::extra::async_op::time_limited (async operations with a time limit);
* so_5::extra::disp::asio_thread_pool. A dispatcher which runs Asio's io_service::run() on a thread pool and schedules execution of event-handler via asio::post() facility.
* so_5::extra::env_infrastructures::asio::simple_mtsafe. An implementation of thread-safe single threaded environment infrastructure on top of Asio;
* so_5::extra::env_infrastructures::asio::simple_not_mtsafe. An implementation of not-thread-safe single threaded environment infrastructure on top of Asio;
* so_5::extra::enveloped_msg. A set of tools for working with enveloped messages.
* so_5::extra::mboxes::collecting_mbox. An implementation of mbox which collects messages of type T and sends bunches of collected messages to the target mbox;
* so_5::extra::mboxes::broadcast::fixed_mbox. An implementation of mbox which broadcasts messages to a set of destination mboxes;
* so_5::extra::mboxes::proxy. A proxy-mbox which delegates all calls to the underlying actual mbox. Such proxy simplifies development of custom mboxes.
* so_5::extra::mboxes::retained_msg. An implementation of mbox which holds the last sent message and automatically resend it to every new subscriber for this message type;
* so_5::extra::mboxes::round_robin. An implementation of *round-robin* mbox which performs delivery of messages by round-robin scheme;
* so_5::extra::revocable_msg. A set of tools for sending messages/signals those can be revoked. 
* so_5::extra::revocable_timer. A set of tools for sending delayed/periodic messages/signals those can be revoked.
* so_5::extra::shutdowner. A tool to simplify prevention of SObjectizer shutdown in cases where some agents require more time for graceful shutdown (like storing caches to disk and stuff like that);
* so_5::extra::sync. A set of tools for performing synchronous interaction between agents (or threads if only mchains are used).

More features can be added to so_5_extra in future. Some of so_5_extra's features can become parts of SObjectizer itself if they will be in wide use.

# Obtaining And Using

## The Old-School Way

### Obtaining 

so_5_extra can be obtained from source-code repository via Hg. For example:

    hg clone https://bitbucket.org/sobjectizerteam/so5extra

so_5_extra can also be downloaded from the corresponding [Downloads](https://bitbucket.org/sobjectizerteam/so5extra/downloads/) section on BitBucket. There are two types of achives with so_5_extra: 

* archives with so_5_extra sources only (with names like `so5extra-1.3.0.tar.xz`);
* archives with so_5_extra and all dependecies, like SObjectizer and Asio. These archives have names like `so5extra-1.3.0-full.tar.xz`).

If so_5_extra is got from repository or downloaded as archive without dependecies inside then obtaining of dependecies could be necessary. It can be done via mxxruexternals command: 

    hg clone https://bitbucket.org/sobjectizerteam/so5extra
    cd so5extra
    mxxruexternals

Note: `mxxruexternals` is a part of Mxx_ru gem. To use Mxx_ru it is necessary to install Ruby and then Mxx_ru gem (by `gem install Mxx_ru`).

### Using

so_5_extra is a header-only library. There is no need to compile and link so_5_extra itself. Only INCLUDE path must be set appropriately.

### Using via CMake

Since v.1.2.1 there is the very basic support for CMake: so_5_extra can be used in CMake-based projects. There is `dev/so_5_extra/CMakeFiles.txt` for that.

But building so_5_extra's examples and/or tests via CMake is not supported.
If you want to have CMake support for these things let us known via "info at stiffstream dot com".

## Obtaining And Using Via vcpkg

Since v.1.2.1 so_5_extra is available via [vcpkg](https://github.com/Microsoft/vcpkg) library manager.

To install so_5_extra it is necessary to use `vcpkg install` command:

    vcpkg install so5extra

Please note that so_5_extra is identified as `so5extra` in vcpkg repository.

To use so_5_extra it is necessary to add the following lines in your CMakeFiles.txt: 

    find_package(so5extra CONFIG REQUIRED)
    find_package(sobjectizer CONFIG REQUIRED)

And then it is necessary to link your target against SObjectizer's libraries:

    target_link_libraries(your_target sobjectizer::SharedLib)
    target_link_libraries(your_target sobjectizer::so5extra)

For example: this is a simple CMakeFiles.txt file for so_5_extra's sample `delivery_receipt`:

    find_package(so5extra CONFIG REQUIRED)
    find_package(sobjectizer CONFIG REQUIRED)

    set(SAMPLE sample.so_5_extra.delivery_receipt)
    add_executable(${SAMPLE} main.cpp)
    target_link_libraries(${SAMPLE} sobjectizer::SharedLib)
    target_link_libraries(${SAMPLE} sobjectizer::so5extra)
    install(TARGETS ${SAMPLE} DESTINATION bin)

## Obtaining And Using Via Conan

### Installing And Adding To conanfile.txt

Since Nov 2018 so_5_extra can be used via [Conan](https://conan.io) dependency manager. To use so_5_extra it is necessary to do the following steps.

Add the corresponding remote to your conan:

    conan remote add stiffstream https://api.bintray.com/conan/stiffstream/public

It can be also necessary to add public-conan remote:

    conan remote add public-conan https://api.bintray.com/conan/bincrafters/public-conan

Add so_5_extra to conanfile.txt of your project:

    [requires]
    so5extra/1.3.0@stiffstream/testing

It also may be necessary to specify shared option for SObjectizer. For example, for build SObjectizer as a static library:

    [options]
    sobjectizer:shared=False

Install dependencies for your project:

    conan install SOME_PATH --build=missing

### Adding so_5_extra To Your CMakeLists.txt

Please note that so_5_extra and SObjectizer should be added to your CMakeLists.txt via find_package command:

    ...
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup()

    find_package(sobjectizer CONFIG REQUIRED)
    find_package(so5extra CONFIG REQUIRED)
    ...
    target_link_libraries(your_target sobjectizer::SharedLib) # Or sobjectizer::StaticLib
    target_link_libraries(your_target sobjectizer::so5extra)


# Building Samples And Tests

To build so_5_extra samples and tests it is necessary to use Ruby and Mxx_ru gem. For example:

    hg clone https://bitbucket.org/sobjectizerteam/so5extra
    cd so5extra
    mxxruexternals
    cd dev
    ruby build.rb

All tests and samples will be built. If it is necessary to build only examples it can be done by:

    hg clone https://bitbucket.org/sobjectizerteam/so5extra
    cd so5extra
    mxxruexternals
    cd dev
    ruby sample/so_5_extra/build_samples.rb

# Building API Reference Manual

API Reference Manual can be build via doxygen. Just go into `dev` subdirectory where `Doxygen` file is located and run `doxygen`. The generated documentation in HTML format will be stored into `doc/html` subdir.

# License

so_5_extra is distributed on dual-license mode. There is a GNU Affero GPL v.3 license for usage of so_5_extra in OpenSource software. There is also a commercial license for usage of so_5_extra in proprietary projects (contact "info at stiffstream dot com" for more information).

For the license of *SObjectizer* library see LICENSE file in *SObjectizer* distributive.

For the license of *asio* library see COPYING file in *asio* distributive.

For the license of *doctest* library see LICENSE file in *doctest* distributive.
