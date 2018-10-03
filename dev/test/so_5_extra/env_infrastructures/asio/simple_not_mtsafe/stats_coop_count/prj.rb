require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj.rb'
	required_prj 'asio_mxxru/prj.rb'
	required_prj 'test/so_5_extra/env_infrastructures/asio/platform_specific_libs.rb'

	target '_unit.test.so_5_extra.env_infrastructure.asio.simple_not_mtsafe.stats_coop_count'

	cpp_source 'main.cpp'
}

