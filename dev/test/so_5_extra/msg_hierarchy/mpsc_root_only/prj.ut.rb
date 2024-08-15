require 'mxx_ru/binary_unittest'

path = 'test/so_5_extra/msg_hierarchy/mpsc_root_only'

MxxRu::setup_target(
	MxxRu::BinaryUnittestTarget.new(
		"#{path}/prj.ut.rb",
		"#{path}/prj.rb" )
)
