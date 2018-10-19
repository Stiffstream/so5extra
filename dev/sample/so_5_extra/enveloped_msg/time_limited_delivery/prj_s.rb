require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj_s.rb'
	target 'sample.so_5_extra.enveloped_msg.time_limited_delivery_s'

	cpp_source 'main.cpp'
}
