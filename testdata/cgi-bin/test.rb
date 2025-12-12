#!/home/eala-lah/local_ruby/bin/ruby

if ENV['CONTENT_LENGTH']
  len = ENV['CONTENT_LENGTH'].to_i
  post_data = STDIN.read(len)
end

print "Content-Type: text/plain\r\n"
print "Status: 200 OK\r\n"
print "\r\n"

puts "--- CGI Ruby Test ---"
puts "Interpreter: Ruby"
puts "Body: #{post_data}"
