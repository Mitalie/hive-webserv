#!/usr/bin/perl

my $len = $ENV{'CONTENT_LENGTH'};
my $post_data;

if ($len) {
    read(STDIN, $post_data, $len);
}

print "Content-Type: text/plain\r\n";
print "Status: 200 OK\r\n";
print "\r\n";

print "--- CGI Perl Test ---\n";
print "Interpreter: Perl\n";
print "Body: $post_data\n";
