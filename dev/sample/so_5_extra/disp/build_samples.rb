#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'sample/so_5_extra/disp'

	required_prj( "#{path}/asio_thread_pool/build_samples.rb" )
}
