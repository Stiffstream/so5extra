require 'mxx_ru/binary_unittest'

path = 'test/so_5_extra/env_infrastructures/asio/simple_mtsafe/unknown_exception_init_fn_3'

MxxRu::setup_target(
	MxxRu::BinaryUnittestTarget.new(
		"#{path}/prj.ut.rb",
		"#{path}/prj.rb" )
)
