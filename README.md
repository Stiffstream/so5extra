# What Is It?

so5extra is a collection of various SObjectizer's extensions. so5extra is built on top of SObjectizer and intended to simplify development of SObjectizer-based applications.

At the current moment so5extra contains the following components:

* so_5::extra::async_op. Several implementation of *async operations*. Contains subcomponents so_5::extra::async_op::time_unlimited (async operations without a limit for execution time) and so_5::extra::async_op::time_limited (async operations with a time limit);
* so_5::extra::disp::asio_one_thread. A dispatcher which runs Asio's io_service::run() on a separate worker thread and schedules execution of event-handler via asio::post() facility;
* so_5::extra::disp::asio_thread_pool. A dispatcher which runs Asio's io_service::run() on a thread pool and schedules execution of event-handler via asio::post() facility;
* so_5::extra::env_infrastructures::asio::simple_mtsafe. An implementation of thread-safe single threaded environment infrastructure on top of Asio;
* so_5::extra::env_infrastructures::asio::simple_not_mtsafe. An implementation of not-thread-safe single threaded environment infrastructure on top of Asio;
* so_5::extra::enveloped_msg. A set of tools for working with enveloped messages;
* so_5::extra::mboxes::broadcast::fixed_mbox. An implementation of mbox which broadcasts messages to a set of destination mboxes;
* so_5::extra::mboxes::collecting_mbox. An implementation of mbox which collects messages of type T and sends bunches of collected messages to the target mbox;
* so_5::extra::mboxes::composite. An implementation of mbox that delegates actual processing of messages to different destination mboxes in dependency of message type.
* so_5::extra::mboxes::inflight_limit. An implementation of mbox that limits the number of "in-flight" messages and drops (discards) new messages if the limit exceeded.
* so_5::extra::mboxes::first_last_subscriber_notification. An implementation of mbox for messages of type T that sends notifications when the first subscriber arrives and the last subscribers leaves;
* so_5::extra::mboxes::proxy. A proxy-mbox which delegates all calls to the underlying actual mbox. Such proxy simplifies development of custom mboxes.
* so_5::extra::mboxes::retained_msg. An implementation of mbox which holds the last sent message and automatically resend it to every new subscriber for this message type;
* so_5::extra::mboxes::round_robin. An implementation of *round-robin* mbox which performs delivery of messages by round-robin scheme;
* so_5::extra::mchains::fixed_size. An implementation of fixed-size mchain which capacity is known at the compile-time;
* so_5::extra::msg_hierarchy. A way to subscribe, receive and handle a message by its base class.
* so_5::extra::revocable_msg. A set of tools for sending messages/signals those can be revoked;
* so_5::extra::revocable_timer. A set of tools for sending delayed/periodic messages/signals those can be revoked;
* so_5::extra::shutdowner. A tool to simplify prevention of SObjectizer shutdown in cases where some agents require more time for graceful shutdown (like storing caches to disk and stuff like that);
* so_5::extra::sync. A set of tools for performing synchronous interaction between agents (or threads if only mchains are used).

More features can be added to so5extra in future. Some of so5extra's features can become parts of SObjectizer itself if they will be in wide use.

# Obtaining And Using

## The Old-School Way

### Obtaining

so5extra can be obtained from source-code repository via Git. For example:

    git clone https://github.com/stiffstream/so5extra

so5extra can also be downloaded from the corresponding [Releases](https://github.com/Stiffstream/so5extra/releases) section on GitHub. Dependencies are downloaded automatically via CMake's FetchContent.

### Building via CMake

Since v.1.6.3 the only way to build so5extra's samples and test is CMake:

```sh
git clone https://github.com/stiffstream/so5extra
cd so5extra
mkdir cmake_build_release
cd cmake_build_release
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

There are several options supported by so5extra's CMakeLists.txt file:

* `SO5EXTRA_BUILD_EXAMPLES` (default `ON`) for building examples;
* `SO5EXTRA_BUILD_TESTS` (default `ON`) for building tests;
* `SO5EXTRA_SANITIZE` (default `OFF`) for using sanitizers in case of GCC or
clang compilers. Supported values: `OFF`, `thread` and `address`. Note that
this option will be transformed into SObjectizer's
`SOBJECTIZER_SANITIZE` option.


## Obtaining And Using Via vcpkg

Since v.1.2.1 so5extra is available via [vcpkg](https://github.com/Microsoft/vcpkg) library manager.

To install so5extra it is necessary to use `vcpkg install` command:

    vcpkg install so5extra

To use so5extra it is necessary to add the following lines in your CMakeFiles.txt: 

    find_package(so5extra CONFIG REQUIRED)
    find_package(sobjectizer CONFIG REQUIRED)

And then it is necessary to link your target against SObjectizer's libraries:

    target_link_libraries(your_target sobjectizer::SharedLib)
    target_link_libraries(your_target sobjectizer::so5extra)

For example: this is a simple CMakeFiles.txt file for so5extra's sample `delivery_receipt`:

    find_package(so5extra CONFIG REQUIRED)
    find_package(sobjectizer CONFIG REQUIRED)

    set(SAMPLE sample.so_5_extra.delivery_receipt)
    add_executable(${SAMPLE} main.cpp)
    target_link_libraries(${SAMPLE} sobjectizer::SharedLib)
    target_link_libraries(${SAMPLE} sobjectizer::so5extra)
    install(TARGETS ${SAMPLE} DESTINATION bin)

## Obtaining And Using Via Conan

### Installing And Adding To conanfile.txt

Since Nov 2018 so5extra can be used via [Conan](https://conan.io) dependency manager. To use so5extra it is necessary to do the following steps.

Add the corresponding remote to your conan:

    conan remote add stiffstream https://api.bintray.com/conan/stiffstream/public

It can be also necessary to add public-conan remote:

    conan remote add public-conan https://api.bintray.com/conan/bincrafters/public-conan

Add so5extra to conanfile.txt of your project:

    [requires]
    so5extra/1.6.0@stiffstream/stable

It also may be necessary to specify shared option for SObjectizer. For example, for build SObjectizer as a static library:

    [options]
    sobjectizer:shared=False

Install dependencies for your project:

    conan install SOME_PATH --build=missing

### Adding so5extra To Your CMakeLists.txt

Please note that so5extra and SObjectizer should be added to your CMakeLists.txt via find_package command:

    ...
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup()

    find_package(sobjectizer CONFIG REQUIRED)
    find_package(so5extra CONFIG REQUIRED)
    ...
    target_link_libraries(your_target sobjectizer::SharedLib) # Or sobjectizer::StaticLib
    target_link_libraries(your_target sobjectizer::so5extra)


# Building API Reference Manual

API Reference Manual can be build via doxygen. Just go into `dev` subdirectory where `Doxygen` file is located and run `doxygen`. The generated documentation in HTML format will be stored into `doc/html` subdir.

# License

Since v.1.4.0 so5extra is distributed under BSD-3-CLAUSE license (see LICENSE file).

For the license of *SObjectizer* library see LICENSE file in *SObjectizer* distributive.

For the license of *asio* library see COPYING file in *asio* distributive.

For the license of *doctest* library see LICENSE file in *doctest* distributive.

