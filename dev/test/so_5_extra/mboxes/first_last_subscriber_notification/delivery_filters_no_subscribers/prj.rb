require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj.rb'

	target '_unit.test.so_5_extra.mboxes.first_last_subscriber_notification.' +
     'delivery_filters_no_subscribers'

	cpp_source 'main.cpp'
}

