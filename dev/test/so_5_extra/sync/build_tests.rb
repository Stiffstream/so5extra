#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/sync'

	required_prj( "#{path}/simple/prj.ut.rb" )
	required_prj( "#{path}/simple/prj_s.ut.rb" )

	required_prj( "#{path}/simple_holder/prj.ut.rb" )
	required_prj( "#{path}/simple_holder/prj_s.ut.rb" )

	required_prj( "#{path}/not_default_constructible/prj.ut.rb" )
	required_prj( "#{path}/not_default_constructible/prj_s.ut.rb" )

	required_prj( "#{path}/moveable_only/prj.ut.rb" )
	required_prj( "#{path}/moveable_only/prj_s.ut.rb" )

	required_prj( "#{path}/simple_signal/prj.ut.rb" )
	required_prj( "#{path}/simple_signal/prj_s.ut.rb" )

	required_prj( "#{path}/no_subscriber/prj.ut.rb" )
	required_prj( "#{path}/no_subscriber/prj_s.ut.rb" )

	required_prj( "#{path}/reply_timeout/prj.ut.rb" )
	required_prj( "#{path}/reply_timeout/prj_s.ut.rb" )

	required_prj( "#{path}/reply_to_mbox/prj.ut.rb" )
	required_prj( "#{path}/reply_to_mbox/prj_s.ut.rb" )

	required_prj( "#{path}/reply_to_agent/prj.ut.rb" )
	required_prj( "#{path}/reply_to_agent/prj_s.ut.rb" )

	required_prj( "#{path}/reply_to_mchain_1/prj.ut.rb" )
	required_prj( "#{path}/reply_to_mchain_1/prj_s.ut.rb" )

	required_prj( "#{path}/reply_to_mchain_2/prj.ut.rb" )
	required_prj( "#{path}/reply_to_mchain_2/prj_s.ut.rb" )
}
