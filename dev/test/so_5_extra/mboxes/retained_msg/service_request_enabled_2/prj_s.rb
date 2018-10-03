require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj_s.rb'

	target '_unit.test.so_5_extra.mboxes.retained_msg.service_request_enabled_2_s'

	cpp_source 'main.cpp'
}

