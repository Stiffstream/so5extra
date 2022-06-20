require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj_s.rb'

	target '_unit.test.so_5_extra.mboxes.inflight_limit.mutable_and_mpmc_s'

	cpp_source 'main.cpp'
}

