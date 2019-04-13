require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj.rb'

	target '_unit.test.so_5_extra.mboxes.retained_msg.service_request_enabled_3'

	cpp_source 'main.cpp'
}

