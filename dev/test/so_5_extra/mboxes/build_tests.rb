#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/mboxes'

	required_prj( "#{path}/round_robin/build_tests.rb" )
	required_prj( "#{path}/collecting_mbox/build_tests.rb" )
	required_prj( "#{path}/retained_msg/build_tests.rb" )
}
