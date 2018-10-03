[TOC]

# What Is It?

so_5_extra is a collection of various SObjectizer's extensions. so_5_extra is built on top of SObjectizer and intended to simplify development of SObjectizer-based applications.

At the current moment so_5_extra contains the following components:

* so_5::extra::async_op. Several implementation of *async operations*. Contains subcomponents so_5::extra::async_op::time_unlimited (async operations without a limit for execution time) and so_5::extra::async_op::time_limited (async operations with a time limit);
* so_5::extra::env_infrastructures::asio::simple_not_mtsafe. An implementation of not-thread-safe single threaded environment infrastructure on top of Asio;
* so_5::extra::env_infrastructures::asio::simple_mtsafe. An implementation of thread-safe single threaded environment infrastructure on top of Asio;
* so_5::extra::mboxes::round_robin. An implementation of *round-robin* mbox which performs delivery of messages by round-robin scheme;
* so_5::extra::mboxes::collecting_mbox. An implementation of mbox which collects messages of type T and sends bunches of collected messages to the target mbox;
* so_5::extra::mboxes::retained_msg. An implementation of mbox which holds the last sent message and automatically resend it to every new subscriber for this message type;
* so_5::extra::shutdowner. A tool to simplify prevention of SObjectizer shutdown in cases where some agents require more time for graceful shutdown (like storing caches to disk and stuff like that);
* so_5::extra::disp::asio_thread_pool. A dispatcher which runs Asio's io_service::run() on a thread pool and schedules execution of event-handler via asio::post() facility.

More features can be added to so_5_extra in future. Some of so_5_extra's features can become parts of SObjectizer itself if they will be in wide use.

# Obtaining And Using

## Obtaining 

so_5_extra can be obtained from source-code repository via Subversion. For example:

    svn export https://svn.code.sf.net/p/sobjectizer/repo/tags/so_5_extra/1.2.0 so_5_extra-1.2.0

so_5_extra can also be downloaded from the corresponding [Files](https://sourceforge.net/projects/sobjectizer/files/sobjectizer/so_5_extra/) section on SourceForge. There are two types of achives with so_5_extra: 

* archives with so_5_extra sources only (with names like `so_5_extra-1.2.0.tar.xz`);
* archives with so_5_extra and all dependecies, like SObjectizer and Asio. These archives have names like `so_5_extra-1.2.0-full.tar.xz`).

If so_5_extra is got from repository or downloaded as archive without dependecies inside then obtaining of dependecies could be necessary. It can be done via mxxruexternals command: 

    svn export https://svn.code.sf.net/p/sobjectizer/repo/tags/so_5_extra/1.2.0 so_5_extra-1.2.0
    cd so_5_extra-1.2.0
    mxxruexternals

Note: `mxxruexternals` is a part of Mxx_ru gem. To use Mxx_ru it is necessary to install Ruby and then Mxx_ru gem (by `gem install Mxx_ru`).

## Using

so_5_extra is a header-only library. There is no need to compile and link so_5_extra itself. Only INCLUDE path must be set appropriately.

## Building Samples And Tests

To build so_5_extra samples and tests it is necessary to use Ruby and Mxx_ru gem. For example:

    svn export https://svn.code.sf.net/p/sobjectizer/repo/tags/so_5_extra/1.2.0 so_5_extra-1.2.0
    cd so_5_extra-1.2.0
    mxxruexternals
    cd dev
    ruby build.rb

All tests and samples will be built. If it is necessary to build only examples it can be done by:

    svn export https://svn.code.sf.net/p/sobjectizer/repo/tags/so_5_extra/1.2.0 so_5_extra-1.2.0
    cd so_5_extra-1.2.0
    mxxruexternals
    cd dev
    ruby sample/so_5_extra/build_samples.rb

## Building API Reference Manual

API Reference Manual can be build via doxygen. Just go into `dev` subdirectory where `Doxygen` file is located and run `doxygen`. The generated documentation in HTML format will be stored into `doc/html` subdir.

# License

so_5_extra is distributed on dual-license mode. There is a GNU Affero GPL v.3 license for usage of so_5_extra in OpenSource software. There is also a commercial license for usage of so_5_extra in proprietary projects (contact "info at stiffstream dot com" for more information).

For the license of *SObjectizer* library see LICENSE file in *SObjectizer* distributive.

For the license of *asio* library see COPYING file in *asio* distributive.

For the license of *doctest* library see LICENSE file in *doctest* distributive.

