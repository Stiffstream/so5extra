require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj.rb'
	target 'sample.so_5_extra.mboxes.collecting_mbox.simple'

	cpp_source 'main.cpp'
}
