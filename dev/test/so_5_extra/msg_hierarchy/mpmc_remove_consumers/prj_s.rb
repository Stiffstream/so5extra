require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj_s.rb'

	target '_unit.test.so_5_extra.msg_hierarchy.mpmc_remove_consumers_s'

	cpp_source 'main.cpp'
}

