require 'mxx_ru/binary_unittest'

path = 'test/so_5_extra/mboxes/first_last_subscriber_notification/mutable_msg'

MxxRu::setup_target(
	MxxRu::BinaryUnittestTarget.new(
		"#{path}/prj_s.ut.rb",
		"#{path}/prj_s.rb" )
)
