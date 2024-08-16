require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj.rb'

	target '_unit.test.so_5_extra.msg_hierarchy.mpmc_remove_consumers'

	cpp_source 'main.cpp'
}

