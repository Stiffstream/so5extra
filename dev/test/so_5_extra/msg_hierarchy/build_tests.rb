#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/msg_hierarchy'

	required_prj( "#{path}/simple/prj.ut.rb" )
	required_prj( "#{path}/simple/prj_s.ut.rb" )

	required_prj( "#{path}/mpmc_mutable_msg/prj.ut.rb" )
	required_prj( "#{path}/mpmc_mutable_msg/prj_s.ut.rb" )
}
