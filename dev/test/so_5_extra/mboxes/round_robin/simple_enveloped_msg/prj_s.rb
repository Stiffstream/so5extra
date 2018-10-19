require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj_s.rb'

	target '_unit.test.so_5_extra.mboxes.round_robin.simple_enveloped_msg_s'

	cpp_source 'main.cpp'
}

