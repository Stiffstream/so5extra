#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/async_op'

	required_prj( "#{path}/time_unlimited/build_tests.rb" )
	required_prj( "#{path}/time_limited/build_tests.rb" )
}
