require 'mxx_ru/binary_unittest'

path = 'test/so_5_extra/mboxes/first_last_subscriber_notification/simple'

MxxRu::setup_target(
	MxxRu::BinaryUnittestTarget.new(
		"#{path}/prj.ut.rb",
		"#{path}/prj.rb" )
)
