require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj_s.rb'
	required_prj 'asio_mxxru/prj.rb'
	required_prj 'test/so_5_extra/env_infrastructures/asio/platform_specific_libs.rb'

	target 'sample.so_5_extra.msg_hierarchy.ping_pong_null_mutex_s'

	cpp_source 'main.cpp'
}
