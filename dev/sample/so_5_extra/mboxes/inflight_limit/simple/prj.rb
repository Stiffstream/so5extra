require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj.rb'
	target 'sample.so_5_extra.mboxes.inflight_limit.simple'

	cpp_source 'main.cpp'
}
