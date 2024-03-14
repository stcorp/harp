# Copyright (C) 2015-2024 S[&]T, The Netherlands.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

from __future__ import print_function

import cffi
import re
import sys

# Import the StringIO class that uses the default str type on each major version
# of Python.
#
# The StringIO.StringIO class is Python 2 specific and works with byte strings,
# i.e. instances of class str.
#
# The io.StringIO class exists in both Python 2 and Python 3, and always uses
# unicode strings, i.e. instances of class unicode on Python 2 and of class str
# on Python 3.
#
try:
    from cStringIO import StringIO
except ImportError:
    try:
        from StringIO import StringIO
    except ImportError:
        from io import StringIO


class Error(Exception):
    pass


class SyntaxError(Error):
    pass


def read_header_file(filename):
    cdefs = StringIO()
    active = False
    with open(filename) as header_file:
        for (line_no, line) in enumerate(header_file):
            if re.match(r"^\s*/\*\s*\*CFFI-ON\*\s*\*/\s*$", line) or re.match(r"^\s*//\s*\*CFFI-ON\*\s*$", line):
                if active:
                    raise SyntaxError("%s:%lu: CFFI-ON marker inside CFFI block" % (filename, line_no))
                active = True
                continue

            if re.match(r"^\s*/\*\s*\*CFFI-OFF\*\s*\*/\s*$", line) or re.match(r"^\s*//\s*\*CFFI-OFF\*\s*$", line):
                if not active:
                    raise SyntaxError("%s:%lu: CFFI-OFF marker outside CFFI block" % (filename, line_no))
                active = False
                continue

            # Remove LIBHARP_API prefix.
            line = re.sub(r"^\s*LIBHARP_API\s*", "", line)

            # Remove brackets from #define statements, since in ABI mode cffi only accepts #define followed by a numeric
            # constant.
            match_obj = re.match(r"^\s*#define\s+(\w+)\s+\(([^)]+)\)\s*$", line)
            if match_obj:
                name = match_obj.group(1)

                try:
                    value = int(match_obj.group(2))
                except ValueError:
                    try:
                        value = float(match_obj.group(2))
                    except ValueError:
                        # If the value cannot be parsed as an int or float, output the original line without
                        # modification.
                        pass
                    else:
                        line = "#define %s %f\n" % (name, value)
                else:
                    line = "#define %s %d\n" % (name, value)

            if active:
                cdefs.write(line)

    if active:
        raise SyntaxError("%s:%lu: unterminated CFFI block; CFFI-OFF marker missing" % (filename, line_no))

    return cdefs.getvalue()


def main(header_path, output_path):
    ffi = cffi.FFI()
    ffi.set_source("_harpc", None)
    ffi.cdef(read_header_file(header_path))
    ffi.emit_python_code(output_path)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("usage: %s <path to harp.h.in> <output Python file>" % sys.argv[0])
        print("generate Python wrapper for the HARP C library using cffi (ABI level, out-of-line)")
        sys.exit(1)

    try:
        main(sys.argv[1], sys.argv[2])
    except Error as _error:
        print("ERROR: %s" % _error)
        sys.exit(1)
