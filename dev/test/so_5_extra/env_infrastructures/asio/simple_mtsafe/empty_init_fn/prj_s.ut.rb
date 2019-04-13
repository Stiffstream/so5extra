require 'mxx_ru/binary_unittest'

path = 'test/so_5_extra/env_infrastructures/asio/simple_mtsafe/empty_init_fn'

MxxRu::setup_target(
	MxxRu::BinaryUnittestTarget.new(
		"#{path}/prj_s.ut.rb",
		"#{path}/prj_s.rb" )
)
