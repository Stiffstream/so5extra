#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/mboxes/composite'

	required_prj( "#{path}/simple/prj.ut.rb" )
	required_prj( "#{path}/simple/prj_s.ut.rb" )

	required_prj( "#{path}/reaction/prj.ut.rb" )
	required_prj( "#{path}/reaction/prj_s.ut.rb" )

	required_prj( "#{path}/mbox_type/prj.ut.rb" )
	required_prj( "#{path}/mbox_type/prj_s.ut.rb" )
}
