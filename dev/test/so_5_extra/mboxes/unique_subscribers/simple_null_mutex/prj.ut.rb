require 'mxx_ru/binary_unittest'

path = 'test/so_5_extra/mboxes/unique_subscribers/simple_null_mutex'

MxxRu::setup_target(
	MxxRu::BinaryUnittestTarget.new(
		"#{path}/prj.ut.rb",
		"#{path}/prj.rb" )
)
