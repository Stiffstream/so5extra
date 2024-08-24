#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/msg_hierarchy'

	required_prj( "#{path}/simple/prj.ut.rb" )
	required_prj( "#{path}/simple/prj_s.ut.rb" )

	required_prj( "#{path}/node_as_root/prj.ut.rb" )
	required_prj( "#{path}/node_as_root/prj_s.ut.rb" )

	required_prj( "#{path}/mpmc_mutable_msg/prj.ut.rb" )
	required_prj( "#{path}/mpmc_mutable_msg/prj_s.ut.rb" )

	required_prj( "#{path}/mpmc_mutable_msg_2/prj.ut.rb" )
	required_prj( "#{path}/mpmc_mutable_msg_2/prj_s.ut.rb" )

	required_prj( "#{path}/mpmc_root_only/prj.ut.rb" )
	required_prj( "#{path}/mpmc_root_only/prj_s.ut.rb" )

	required_prj( "#{path}/mpmc_remove_consumers/prj.ut.rb" )
	required_prj( "#{path}/mpmc_remove_consumers/prj_s.ut.rb" )

	required_prj( "#{path}/mpmc_several_consumers/prj.ut.rb" )
	required_prj( "#{path}/mpmc_several_consumers/prj_s.ut.rb" )

	required_prj( "#{path}/mpmc_redirect/prj.ut.rb" )
	required_prj( "#{path}/mpmc_redirect/prj_s.ut.rb" )

	required_prj( "#{path}/mpsc_simple/prj.ut.rb" )
	required_prj( "#{path}/mpsc_simple/prj_s.ut.rb" )

	required_prj( "#{path}/mpsc_root_only/prj.ut.rb" )
	required_prj( "#{path}/mpsc_root_only/prj_s.ut.rb" )

	required_prj( "#{path}/mpsc_several_consumers_1/prj.ut.rb" )
	required_prj( "#{path}/mpsc_several_consumers_1/prj_s.ut.rb" )

	required_prj( "#{path}/mpsc_several_consumers_2/prj.ut.rb" )
	required_prj( "#{path}/mpsc_several_consumers_2/prj_s.ut.rb" )

	required_prj( "#{path}/mpsc_redirect/prj.ut.rb" )
	required_prj( "#{path}/mpsc_redirect/prj_s.ut.rb" )
}
