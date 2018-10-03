require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj_s.rb'
	required_prj 'asio_mxxru/prj.rb'
	target 'sample.so_5_extra.env_infrastructures.asio.resolve_n_s'

	cpp_source 'main.cpp'

	if 'mswin' == toolset.tag( 'target_os' )
		lib 'wsock32' 
		lib 'ws2_32' 
	end
}
