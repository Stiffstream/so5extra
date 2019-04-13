require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj.rb'

	target '_unit.test.so_5_extra.async_op.time_limited.simple_no_user_default_timeout'

	cpp_source 'main.cpp'
}

