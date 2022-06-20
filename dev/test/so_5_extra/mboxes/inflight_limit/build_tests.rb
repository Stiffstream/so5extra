#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/mboxes/inflight_limit'

	required_prj( "#{path}/simple/prj.ut.rb" )
	required_prj( "#{path}/simple/prj_s.ut.rb" )

	required_prj( "#{path}/simple_mutable/prj.ut.rb" )
	required_prj( "#{path}/simple_mutable/prj_s.ut.rb" )

	required_prj( "#{path}/mutable_and_mpmc/prj.ut.rb" )
	required_prj( "#{path}/mutable_and_mpmc/prj_s.ut.rb" )
}

