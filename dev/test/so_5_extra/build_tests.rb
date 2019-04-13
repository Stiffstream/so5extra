#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra'

	required_prj( "#{path}/env_infrastructures/build_tests.rb" )

	required_prj( "#{path}/mboxes/build_tests.rb" )

	required_prj( "#{path}/shutdowner/build_tests.rb" )

	required_prj( "#{path}/disp/build_tests.rb" )

	required_prj( "#{path}/async_op/build_tests.rb" )

	required_prj( "#{path}/revocable_msg/build_tests.rb" )

	required_prj( "#{path}/revocable_timer/build_tests.rb" )

	required_prj( "#{path}/enveloped_msg/build_tests.rb" )
}
