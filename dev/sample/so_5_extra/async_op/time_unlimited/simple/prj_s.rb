require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj_s.rb'
	target 'sample.so_5_extra.async_op.time_unlimited.simple_s'

	cpp_source 'main.cpp'
}
