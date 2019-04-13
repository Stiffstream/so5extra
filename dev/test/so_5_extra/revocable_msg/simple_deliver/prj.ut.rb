require 'mxx_ru/binary_unittest'

path = 'test/so_5_extra/revocable_msg/simple_deliver'

MxxRu::setup_target(
	MxxRu::BinaryUnittestTarget.new(
		"#{path}/prj.ut.rb",
		"#{path}/prj.rb" )
)
