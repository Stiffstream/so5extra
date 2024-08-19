#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'sample/so_5_extra'

	required_prj( "#{path}/env_infrastructures/build_samples.rb" )
	required_prj( "#{path}/mboxes/build_samples.rb" )
	required_prj( "#{path}/mchains/build_samples.rb" )
	required_prj( "#{path}/shutdowner/build_samples.rb" )
	required_prj( "#{path}/disp/build_samples.rb" )
	required_prj( "#{path}/async_op/build_samples.rb" )
	required_prj( "#{path}/revocable_timer/build_samples.rb" )
	required_prj( "#{path}/enveloped_msg/build_samples.rb" )
	required_prj( "#{path}/sync/build_samples.rb" )
	required_prj( "#{path}/msg_hierarchy/build_samples.rb" )
}
