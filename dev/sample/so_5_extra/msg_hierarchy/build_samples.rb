#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'sample/so_5_extra/msg_hierarchy'

	required_prj( "#{path}/ping_pong/prj.rb" )
	required_prj( "#{path}/ping_pong/prj_s.rb" )

	required_prj( "#{path}/ping_pong_null_mutex/prj.rb" )
	required_prj( "#{path}/ping_pong_null_mutex/prj_s.rb" )
}
