MxxRu::arch_externals :so5 do |e|
  e.url 'https://bitbucket.org/sobjectizerteam/sobjectizer/downloads/so-5.6.0-b2.zip'

  e.map_dir 'dev/so_5' => 'dev'
end

MxxRu::arch_externals :asio do |e|
  e.url 'https://github.com/chriskohlhoff/asio/archive/asio-1-12-2.tar.gz'

  e.map_dir 'asio/include' => 'dev/asio'
end

MxxRu::arch_externals :asio_mxxru do |e|
  e.url 'https://bitbucket.org/sobjectizerteam/asio_mxxru-1.1/get/1.1.1.tar.bz2'

  e.map_dir 'dev/asio_mxxru' => 'dev'
end

MxxRu::arch_externals :doctest do |e|
  e.url 'https://github.com/onqtam/doctest/archive/2.3.1.tar.gz'

  e.map_file 'doctest/doctest.h' => 'dev/doctest/*'
end
