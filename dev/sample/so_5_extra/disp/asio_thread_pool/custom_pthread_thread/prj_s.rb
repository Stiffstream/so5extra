require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj_s.rb'
	required_prj 'asio_mxxru/prj.rb'

	target 'sample.so_5_extra.disp.asio_thread_pool.custom_pthread_thread_s'

	cpp_source 'main.cpp'
}
