require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj_s.rb'

	target '_unit.test.so_5_extra.mboxes.first_last_subscriber_notification.simple_s'

	cpp_source 'main.cpp'
}

