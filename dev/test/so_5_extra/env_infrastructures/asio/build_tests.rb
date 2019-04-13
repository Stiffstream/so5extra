#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/env_infrastructures/asio'

	required_prj( "#{path}/simple_not_mtsafe/build_tests.rb" )

	required_prj( "#{path}/simple_mtsafe/build_tests.rb" )
}
