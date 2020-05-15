#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'sample/so_5_extra/disp/asio_one_thread'

	required_prj( "#{path}/resolve_n/prj.rb" )
	required_prj( "#{path}/resolve_n/prj_s.rb" )
}
