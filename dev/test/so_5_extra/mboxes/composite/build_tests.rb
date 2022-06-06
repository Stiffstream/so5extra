#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/mboxes/composite'

	required_prj( "#{path}/illegal_usage/prj.ut.rb" )
	required_prj( "#{path}/illegal_usage/prj_s.ut.rb" )
}
