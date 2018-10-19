require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj.rb'

	target '_unit.test.so_5_extra.revocable_timer.resend_mutable_periodic'

	cpp_source 'main.cpp'
}

