#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'sample/so_5_extra/env_infrastructures/asio'

	required_prj( "#{path}/resolve_n/prj.rb" )
	required_prj( "#{path}/resolve_n/prj_s.rb" )

	required_prj( "#{path}/resolve_interactive/prj.rb" )
	required_prj( "#{path}/resolve_interactive/prj_s.rb" )
}
