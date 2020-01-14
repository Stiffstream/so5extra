MxxRu::git_externals :so5 do |e|
  e.url 'https://github.com/Stiffstream/sobjectizer'
  e.commit '69bbd194'

  e.map_dir 'dev/so_5' => 'dev'
end

=begin
MxxRu::arch_externals :so5 do |e|
  e.url 'https://github.com/Stiffstream/sobjectizer/archive/v.5.7.0-b1.tar.gz'

  e.map_dir 'dev/so_5' => 'dev'
end
=end

MxxRu::arch_externals :asio do |e|
  e.url 'https://github.com/chriskohlhoff/asio/archive/asio-1-14-0.tar.gz'

  e.map_dir 'asio/include' => 'dev/asio'
end

MxxRu::arch_externals :asio_mxxru do |e|
  e.url 'https://github.com/Stiffstream/asio_mxxru/archive/1.1.2.tar.gz'

  e.map_dir 'dev/asio_mxxru' => 'dev'
end

MxxRu::arch_externals :doctest do |e|
  e.url 'https://github.com/onqtam/doctest/archive/2.3.6.tar.gz'

  e.map_file 'doctest/doctest.h' => 'dev/doctest/*'
end

