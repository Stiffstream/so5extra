require 'mxx_ru/binary_unittest'

path = 'test/so_5_extra/env_infrastructures/asio/simple_mtsafe/autoshutdown_disabled'

MxxRu::setup_target(
	MxxRu::BinaryUnittestTarget.new(
		"#{path}/prj_s.ut.rb",
		"#{path}/prj_s.rb" )
)
