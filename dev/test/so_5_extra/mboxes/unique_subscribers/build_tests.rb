#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/mboxes/unique_subscribers'

	required_prj( "#{path}/simple/prj.ut.rb" )
	required_prj( "#{path}/simple/prj_s.ut.rb" )

	required_prj( "#{path}/simple_null_mutex/prj.ut.rb" )
	required_prj( "#{path}/simple_null_mutex/prj_s.ut.rb" )

	required_prj( "#{path}/repeated_subscribe/prj.ut.rb" )
	required_prj( "#{path}/repeated_subscribe/prj_s.ut.rb" )
}
