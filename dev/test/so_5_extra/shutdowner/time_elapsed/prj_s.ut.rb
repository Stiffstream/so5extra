require 'mxx_ru/binary_unittest'

path = 'test/so_5_extra/shutdowner/time_elapsed'

MxxRu::setup_target(
	MxxRu::NegativeBinaryUnittestTarget.new(
		"#{path}/prj_s.ut.rb",
		"#{path}/prj_s.rb" )
)
