require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj.rb'

	target '_unit.test.so_5_extra.shutdowner.subscribe_when_shutdown'

	cpp_source 'main.cpp'
}

