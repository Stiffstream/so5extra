#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/mboxes/first_last_subscriber_notification'

	required_prj( "#{path}/simple/prj.ut.rb" )
	required_prj( "#{path}/simple/prj_s.ut.rb" )

	required_prj( "#{path}/simple_null_mutex/prj.ut.rb" )
	required_prj( "#{path}/simple_null_mutex/prj_s.ut.rb" )

	required_prj( "#{path}/mpsc_multi_subscribers/prj.ut.rb" )
	required_prj( "#{path}/mpsc_multi_subscribers/prj_s.ut.rb" )

	required_prj( "#{path}/delivery_filters_no_subscribers/prj.ut.rb" )
	required_prj( "#{path}/delivery_filters_no_subscribers/prj_s.ut.rb" )

	required_prj( "#{path}/delivery_filters_and_subscribers/prj.ut.rb" )
	required_prj( "#{path}/delivery_filters_and_subscribers/prj_s.ut.rb" )
}
