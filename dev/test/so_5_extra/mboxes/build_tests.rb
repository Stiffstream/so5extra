#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/mboxes'

	required_prj( "#{path}/round_robin/build_tests.rb" )
	required_prj( "#{path}/collecting_mbox/build_tests.rb" )
	required_prj( "#{path}/retained_msg/build_tests.rb" )
	required_prj( "#{path}/proxy/build_tests.rb" )
	required_prj( "#{path}/broadcast/build_tests.rb" )
	required_prj( "#{path}/unique_subscribers/build_tests.rb" )
	required_prj( "#{path}/first_last_subscriber_notification/build_tests.rb" )
	required_prj( "#{path}/composite/build_tests.rb" )
	required_prj( "#{path}/inflight_limit/build_tests.rb" )
}
