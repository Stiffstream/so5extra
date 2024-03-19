MxxRu::arch_externals :so5 do |e|
#  e.url 'https://github.com/Stiffstream/sobjectizer/archive/v.5.8.1.tar.gz'
  e.url 'https://github.com/Stiffstream/sobjectizer/archive/3d47a8b38633b798113e988744fda19c1d54f439.tar.gz'

  e.map_dir 'dev/so_5' => 'dev'
end

MxxRu::arch_externals :asio do |e|
  e.url 'https://github.com/chriskohlhoff/asio/archive/asio-1-28-0.tar.gz'

  e.map_dir 'asio/include' => 'dev/asio'
end

MxxRu::arch_externals :asio_mxxru do |e|
  e.url 'https://github.com/Stiffstream/asio_mxxru/archive/1.1.2.tar.gz'

  e.map_dir 'dev/asio_mxxru' => 'dev'
end

MxxRu::arch_externals :doctest do |e|
  e.url 'https://github.com/doctest/doctest/archive/refs/tags/v2.4.8.tar.gz'

  e.map_file 'doctest/doctest.h' => 'dev/doctest/*'
end

