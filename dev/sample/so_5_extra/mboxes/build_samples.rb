#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'sample/so_5_extra/mboxes'

	required_prj( "#{path}/round_robin/build_samples.rb" )
	required_prj( "#{path}/collecting_mbox/build_samples.rb" )
	required_prj( "#{path}/retained_msg/build_samples.rb" )
	required_prj( "#{path}/broadcast/build_samples.rb" )
}
