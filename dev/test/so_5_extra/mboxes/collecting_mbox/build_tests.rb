#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/mboxes/collecting_mbox'

	required_prj( "#{path}/illegal_usage/prj.ut.rb" )
	required_prj( "#{path}/illegal_usage/prj_s.ut.rb" )

	required_prj( "#{path}/simple/prj.ut.rb" )
	required_prj( "#{path}/simple/prj_s.ut.rb" )

	required_prj( "#{path}/simple_enveloped_msg/prj.ut.rb" )
	required_prj( "#{path}/simple_enveloped_msg/prj_s.ut.rb" )

	required_prj( "#{path}/simple_for_each/prj.ut.rb" )
	required_prj( "#{path}/simple_for_each/prj_s.ut.rb" )

	required_prj( "#{path}/simple_for_each_enveloped_msg/prj.ut.rb" )
	required_prj( "#{path}/simple_for_each_enveloped_msg/prj_s.ut.rb" )

	required_prj( "#{path}/simple_for_each_with_index/prj.ut.rb" )
	required_prj( "#{path}/simple_for_each_with_index/prj_s.ut.rb" )

	required_prj( "#{path}/simple_signal/prj.ut.rb" )
	required_prj( "#{path}/simple_signal/prj_s.ut.rb" )

	required_prj( "#{path}/simple_n/prj.ut.rb" )
	required_prj( "#{path}/simple_n/prj_s.ut.rb" )

	required_prj( "#{path}/simple_n_signal/prj.ut.rb" )
	required_prj( "#{path}/simple_n_signal/prj_s.ut.rb" )

	required_prj( "#{path}/simple_mutable/prj.ut.rb" )
	required_prj( "#{path}/simple_mutable/prj_s.ut.rb" )

	required_prj( "#{path}/simple_mutable_for_each/prj.ut.rb" )
	required_prj( "#{path}/simple_mutable_for_each/prj_s.ut.rb" )

	required_prj( "#{path}/simple_mutable_for_each_enveloped_msg/prj.ut.rb" )
	required_prj( "#{path}/simple_mutable_for_each_enveloped_msg/prj_s.ut.rb" )

	required_prj( "#{path}/simple_mutable_for_each_with_index/prj.ut.rb" )
	required_prj( "#{path}/simple_mutable_for_each_with_index/prj_s.ut.rb" )

	required_prj( "#{path}/with_nth_noncopyable_ref/prj.ut.rb" )
	required_prj( "#{path}/with_nth_noncopyable_ref/prj_s.ut.rb" )
}
