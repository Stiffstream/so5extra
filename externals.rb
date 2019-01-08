MxxRu::arch_externals :so5 do |e|
  e.url 'https://sourceforge.net/projects/sobjectizer/files/sobjectizer/SObjectizer%20Core%20v.5.5/so-5.5.24.zip'
  e.sha512 'd33b5660fb6d7b1182d68069bb86782dd7632071723762875d88dd6d67353f6913b25e0574d87e06b86f472608631f59c3e26dc9c67cda9f8223454ae758b524'

  e.map_dir 'dev/so_5' => 'dev'
  e.map_dir 'dev/timertt' => 'dev'
  e.map_dir 'dev/various_helpers_1' => 'dev'
end

MxxRu::arch_externals :asio do |e|
  e.url 'https://github.com/chriskohlhoff/asio/archive/asio-1-12-1.tar.gz'
  e.sha1 '167b3470f5943e5f8601486023acd2bc6e5ca846'

  e.map_dir 'asio/include' => 'dev/asio'
end

MxxRu::arch_externals :asio_mxxru do |e|
  e.url 'https://bitbucket.org/sobjectizerteam/asio_mxxru-1.1/get/1.1.1.tar.bz2'

  e.map_dir 'dev/asio_mxxru' => 'dev'
end

MxxRu::arch_externals :doctest do |e|
  e.url 'https://github.com/onqtam/doctest/archive/2.2.0.tar.gz'

  e.map_file 'doctest/doctest.h' => 'dev/doctest/*'
end
