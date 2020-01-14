#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'sample/so_5_extra/mchains'

	required_prj( "#{path}/fixed_size/build_samples.rb" )
}

