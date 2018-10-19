MxxRu::arch_externals :so5 do |e|
  e.url 'https://sourceforge.net/projects/sobjectizer/files/sobjectizer/SObjectizer%20Core%20v.5.5/so-5.5.23-beta1.zip'
  e.sha512 '3ed542804162cc6d87259954c586077afac8269861d1076126f18772ccef19513e245707a40f8b7a6c0d30a20134c3b37a80b1c2a4301edca6f50297799fbb67'

  e.map_dir 'dev/so_5' => 'dev'
  e.map_dir 'dev/timertt' => 'dev'
  e.map_dir 'dev/various_helpers_1' => 'dev'
end

MxxRu::arch_externals :asio do |e|
  e.url 'https://github.com/chriskohlhoff/asio/archive/asio-1-12-0.tar.gz'
  e.sha1 '630580c8393edafa63e7edfad953a03fba9afb80'

  e.map_dir 'asio/include' => 'dev/asio'
end

MxxRu::arch_externals :asio_mxxru do |e|
  e.url 'https://bitbucket.org/sobjectizerteam/asio_mxxru-1.1/get/1.1.1.tar.bz2'

  e.map_dir 'dev/asio_mxxru' => 'dev'
end

MxxRu::arch_externals :doctest do |e|
  e.url 'https://github.com/onqtam/doctest/archive/2.0.0.tar.gz'

  e.map_file 'doctest/doctest.h' => 'dev/doctest/*'
end
