require 'mxx_ru/binary_unittest'

path = 'test/so_5_extra/mboxes/first_last_subscriber_notification/' +
  'delivery_filters_and_subscribers'

MxxRu::setup_target(
	MxxRu::BinaryUnittestTarget.new(
		"#{path}/prj.ut.rb",
		"#{path}/prj.rb" )
)
