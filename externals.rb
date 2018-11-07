MxxRu::arch_externals :so5 do |e|
  e.url 'https://sourceforge.net/projects/sobjectizer/files/sobjectizer/SObjectizer%20Core%20v.5.5/so-5.5.23.zip'
  e.sha512 '61c2b9e42d88eafef67b38a1b517af7cbda131835d8ae60c914bd89d21e84801278292064c7ad823c0b31a376b0db68f1ee4a7e87892c2f166c39e8068d86122'

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
