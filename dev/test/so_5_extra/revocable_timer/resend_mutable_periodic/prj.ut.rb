require 'mxx_ru/binary_unittest'

path = 'test/so_5_extra/revocable_timer/resend_mutable_periodic'

MxxRu::setup_target(
	MxxRu::BinaryUnittestTarget.new(
		"#{path}/prj.ut.rb",
		"#{path}/prj.rb" )
)
