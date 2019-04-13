require 'mxx_ru/binary_unittest'

path = 'test/so_5_extra/mboxes/collecting_mbox/with_nth_noncopyable_ref'

MxxRu::setup_target(
	MxxRu::BinaryUnittestTarget.new(
		"#{path}/prj_s.ut.rb",
		"#{path}/prj_s.rb" )
)
