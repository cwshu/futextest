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

# Test for a color capable console
if [ -z "$USE_COLOR" ]; then
    tput setf 7
    if [ $? -eq 0 ]; then
        USE_COLOR=1
        tput sgr0
    fi
fi
if [ "$USE_COLOR" -eq 1 ]; then
    COLOR="-c"
fi

for THREADS in 1 2 3 4 5 6 8 10 12 16 24 32 64 128 256 512 1024; do
    ./futex_wait $COLOR -n $THREADS
done

exit 0
