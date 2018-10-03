require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj_s.rb'

	target '_unit.test.so_5_extra.async_op.time_limited.several_completion_s'

	cpp_source 'main.cpp'
}

