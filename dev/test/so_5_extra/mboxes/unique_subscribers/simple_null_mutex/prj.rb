require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj.rb'

	target '_unit.test.so_5_extra.mboxes.unique_subscribers.simple_null_mutex'

	cpp_source 'main.cpp'
}
