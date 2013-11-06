#!/usr/bin/perl

print "{\n\tmailboxes = (\n";
$first = 1;

while (<STDIN>)
{
  if (/^\s*box (.*)/)
  {
    if (!$first)
    {
      print "\t},\n";
    }
    $first = 0;
    print "\t{\n";
    print "\t\tpath = \"$1\"\;\n";
  }
  elsif (/^\s*title (.*)/)
  {
    $out = $1;
    $out =~ s/\"/\\\"/g;
    print "\t\ttitle = \"$out\"\;\n";
  }
  elsif (/^\s*command (.*)/)
  {
    $out = $1;
    $out =~ s/\"/\\\"/g;
    print "\t\tcommand = \"$out\"\;\n";
  }
  elsif (/^\s*nobeep/)
  {
    print "\t\tbeep = false\;\n";
  }
  elsif (/^\s*headertime (.*)/)
  {
    print "\t\theadertime = $1\;\n";
  }
  elsif (/^\s*poll (.*)/)
  {
    print "\t\tpolltime = $1\;\n";
  }
  elsif (/^\s*newsbox/)
  {
    print "\t\ttype = nntp\;\n";
  }
}
print "\t}\n);\n}\n";
