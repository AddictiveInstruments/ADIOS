#!/usr/bin/perl
##################################################################################################
# $Id: gen_inc_file.pl 1191 2011-04-25 19:06:07Z tk $
# This perl script generates an include file which contains the BSL code
# Usage:   perl gen_inc_file.pl <bin-file> <inc-file> <array-name>
# Example: perl gen_inc_file.pl project.bin mios32_bsl_test.inc mios32_bsl_test_image
##################################################################################################

use Fcntl;
use Getopt::Long;

my $calc_lpc17_checksum;
my $size_opt;

GetOptions (
   "reduced_bsl" => \$reduced_bsl,
   "size=i" => \$size_opt,
   );

if( scalar(@ARGV) != 4 ) {
  die "Usage:   perl gen_inc_file.pl <bin-file> <inc-file> <array-name> <code-section> [-reduced_bsl] [-size=<bytes>]\n" .
      "Example: perl gen_inc_file.pl project.bin mios32_bsl_test.inc mios32_bsl_test_code mios32_bsl\n";
}

my ($bin_file, $inc_file, $array_name, $code_section) = @ARGV;
my $array_declaration = $code_section ? "__attribute__ ((section(\".${code_section}\"))) const u8" : "const u8";
my $dump_size = 0x4000; # reserved for BSL and EEPROM emulation

if ( $reduced_bsl ) {$dump_size = 0x2800; }  # reserved for reduced BSL (STM32G0xx)
if ( $size_opt ) {$dump_size = $size_opt; }  # explicit size (e.g. computed by gen_bsl_boundary.sh) overrides the above

# read .bin file into $content array
open(IN, "<${bin_file}") || die "ERROR: '${bin_file}' does not exist!\n";
print "Reading '${bin_file}'...\n";
my $len;
my $content;
if( ($len=sysread(IN, $content, $dump_size)) <= 0 ) {
  die "ERROR: Files longer than ${dump_size} are not supported (Length: ${len} bytes)\n";
}
close(IN);

# convert to C array
print "Writing out to '${inc_file}'\n";
open(OUT, ">${inc_file}") || die "ERROR: cannot open '${inc_file}'!\n";

printf OUT "// \$Id: \$\n";
printf OUT "// generated with '$0 " . join(" ", @ARGV) . "'\n\n";
printf OUT "${array_declaration} ${array_name}[${dump_size}] = {\n", $len;
my $line = "";
# MIOS32_SYS_ADDR_FASTBOOT_CONFIRM / MIOS32_SYS_ADDR_FASTBOOT: last 256 bytes
# before the boundary (MIOS32_SYS_ADDR_BSL_INFO_BEGIN + 0xd2/0xd3), relative
# to $dump_size so this stays correct for any computed boundary, not just the
# historical fixed 0x2800 (see include/mios32/mios32_sys.h)
my $fastboot_confirm_offset = $dump_size - 0x2e;
my $fastboot_offset = $dump_size - 0x2d;
for($i=0; $i<$dump_size; ++$i) {
  my $b = ($i >= $len) ? 0xff : ord(substr($content, $i, 1));
  if($i==$fastboot_confirm_offset){
    $b =0x42;
  } elsif($i==$fastboot_offset){
    $b =0x01;
  }
  $line .= sprintf("0x%02x,", $b);
  if( ($i % 16) == 15 ) {
    print OUT "$line\n";
    $line = "";
  }
}
if( length($line) ) {
  print OUT "$line\n";
}
printf OUT "};\n";

close(OUT);
