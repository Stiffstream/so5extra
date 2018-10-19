require 'mxx_ru/binary_unittest'

path = 'test/so_5_extra/mboxes/collecting_mbox/simple_mutable_for_each_enveloped_msg'

MxxRu::setup_target(
	MxxRu::BinaryUnittestTarget.new(
		"#{path}/prj.ut.rb",
		"#{path}/prj.rb" )
)
