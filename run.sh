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
#      Run all tests under the functional, performance, and stress directories.
#      Format and summarize the results.
#
# AUTHOR
#      Darren Hart <dvhltc@us.ibm.com>
#
# HISTORY
#      2009-Nov-9: Initial version by Darren Hart <dvhltc@us.ibm.com>
#
###############################################################################

COLOR=""
tput setf 7
if [ $? -eq 0 ]; then
    COLOR="-c"
    tput sgr0
fi
export COLOR

(cd functional; ./run.sh)
(cd performance; ./run.sh)
(cd stress; ./run.sh)
