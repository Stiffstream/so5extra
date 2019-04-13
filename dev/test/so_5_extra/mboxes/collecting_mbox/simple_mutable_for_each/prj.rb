require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj.rb'

	target '_unit.test.so_5_extra.mboxes.collecting_mbox.simple_mutable_for_each'

	cpp_source 'main.cpp'
}

