#!/usr/bin/perl
#
# Copyright (C) 2007 SIPfoundry Inc.
# Licensed by SIPfoundry under the LGPL license.
#
# Copyright (C) 2007 Pingtel Corp.
# Licensed to SIPfoundry under a Contributor Agreement.
#

# Given a list of files, look for IPC elements (semaphores and shared
# memory segments) and optionally remove them.  Optionally delete the
# files as well.
#
# Useful for cleaning up after processes that tie shared memory and semaphores
# to files, a common SysV practice that happens when the ftok()function is 
# used to generate a key.  When the file is removed, if the IPC elements are 
# left behind then it becomes possible for a new file to get the same key,
# and thus end up using a previously created IPC element, that may not be
# of the correct size or kind.
#

use Getopt::Long ;
use IPC::SysV qw(IPC_RMID) ;

$verbose = 0 ;
$remove = 0 ;
$delete = 0 ;
$project = 48 ; # ASCII '0' (imdb uses '0' for all it's projects)

GetOptions("verbose" => \$verbose, 
           "remove" => \$remove, 
           "delete" => \$delete,
           "project=i" => \$project) ;
if (@ARGV == 0)
{
   $usage = <<'EOT';
Usage: [--verbose --remove --delete -project n] file [file...]
        -v{erbose}  verbose
        -r{remove}  remove semaphore and shared memory
        -d{delete}  delete file if semaphore and/or shared memory removed
        -p{roject}  the proj_id given to ftok(3).  The default is 48 (ASCII '0')
EOT
   die $usage ;
}

print "project id is $project\n" if $verbose ;

for my $file (@ARGV) 
{
   $key = IPC::SysV::ftok($file, $project) ;
   $semid = semget($key, 0, 0) ;
  
  $unlink = 0 ;
   if (defined($semid))
   { 
      print "semid $semid matches path $file\n" if $verbose ;

      if ($remove)
      {
         semctl($semid, 0, IPC_RMID, undef) ;
         $unlink = 1 ;
      }
   }
   else
   {
      print "semid not found for path $file\n" if $verbose ;
   }

   $shmid = shmget($key, 0, 0) ;
   if (defined($semid))
   { 
      print "shmid $shmid matches path $file\n" if $verbose ;

      if ($remove)
      {
         shmctl($semid, IPC_RMID, undef) ;
         $unlink = 1 ;
      }
   }
   else
   {
      print "shmid not found for path $file\n" if $verbose ;
   }

   unlink $file if ($delete && $unlink) ;
}