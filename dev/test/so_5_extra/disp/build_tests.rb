#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/disp'

	required_prj "#{path}/asio_thread_pool/build_tests.rb"

	required_prj "#{path}/asio_one_thread/build_tests.rb"
}

