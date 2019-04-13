require 'mxx_ru/binary_unittest'

path = 'test/so_5_extra/enveloped_msg/send_mutable'

MxxRu::setup_target(
	MxxRu::BinaryUnittestTarget.new(
		"#{path}/prj.ut.rb",
		"#{path}/prj.rb" )
)
