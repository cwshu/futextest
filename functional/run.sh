#!/bin/sh

###############################################################################
#
#   Copyright © International Business Machines  Corp., 2009
#
#   This program is free software;  you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY;  without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
#   the GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program;  if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#
# NAME
#      run.sh
#
# DESCRIPTION
#      Run tests in the current directory.
#
# AUTHOR
#      Darren Hart <dvhltc@us.ibm.com>
#
# HISTORY
#      2009-Nov-9: Initial version by Darren Hart <dvhltc@us.ibm.com>
#
###############################################################################

echo 
# requeue pi testing
# without timeouts
./futex_requeue_pi
./futex_requeue_pi -c -b
./futex_requeue_pi -c -b -l
./futex_requeue_pi -c -b -o
./futex_requeue_pi -c -l
./futex_requeue_pi -c -o
# with timeouts
./futex_requeue_pi -c -b -l -t 5000
./futex_requeue_pi -c -l -t 5000
./futex_requeue_pi -c -b -l -t 500000
./futex_requeue_pi -c -l -t 500000
./futex_requeue_pi -c -b -t 5000
./futex_requeue_pi -c -t 5000
./futex_requeue_pi -c -b -t 500000
./futex_requeue_pi -c -t 500000
./futex_requeue_pi -c -b -o -t 5000
./futex_requeue_pi -c -l -t 5000
./futex_requeue_pi -c -b -o -t 500000
./futex_requeue_pi -c -l -t 500000
# with long timeout
./futex_requeue_pi -c -b -l -t 2000000000
./futex_requeue_pi -c -l -t 2000000000


echo 
./futex_requeue_pi_mismatched_ops -c

echo 
./futex_requeue_pi_signal_restart -c

echo 
./futex_wait_timeout -c
