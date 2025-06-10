#
#  Hook to disable the confirmation prompt when quitting GDB.
#
define hook-quit
    set confirm off
end

#
#   Start logging to file.
#
define start-file-logging
  set logging file gdblog.txt
  set logging enabled on
  set trace-commands on
end

#
#   Stop logging to file.
#
define stop-file-logging
  set logging enabled off
  set trace-commands off
end

hook-quit
flushregs
set remote hardware-watchpoint-limit 2
set listsize 30

set output-radix 16
