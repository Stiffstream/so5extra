require 'mxx_ru/binary_unittest'

path = 'test/so_5_extra/async_op/time_limited/ensure_destroyed_3'

MxxRu::setup_target(
	MxxRu::BinaryUnittestTarget.new(
		"#{path}/prj_s.ut.rb",
		"#{path}/prj_s.rb" )
)
