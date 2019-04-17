require 'mxx_ru/binary_unittest'

path = 'test/so_5_extra/disp/asio_thread_pool/agent_ring_2'

MxxRu::setup_target(
	MxxRu::BinaryUnittestTarget.new(
		"#{path}/prj.ut.rb",
		"#{path}/prj.rb" )
)
