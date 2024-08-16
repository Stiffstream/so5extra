require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj.rb'

	target '_unit.test.so_5_extra.msg_hierarchy.mpmc_several_consumers'

	cpp_source 'main.cpp'
	cpp_source 'sender.cpp'
	cpp_source 'stopper.cpp'
	cpp_source 'receivers.cpp'
}

