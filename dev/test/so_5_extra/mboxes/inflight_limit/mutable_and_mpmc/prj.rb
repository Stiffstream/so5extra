require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj.rb'

	target '_unit.test.so_5_extra.mboxes.inflight_limit.mutable_and_mpmc'

	cpp_source 'main.cpp'
}

