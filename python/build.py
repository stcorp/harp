import cffi
import re
import StringIO
import sys

def read_header_file(filename):
    cdefs = StringIO.StringIO()
    active = False
    with open(filename) as header_file:
        for line in header_file:
            if re.match(r"^\s*/\*\s*\*CFFI-ON\*\s*\*/\s*$", line) or re.match(r"^\s*//\s*\*CFFI-ON\*\s*$", line):
                assert(not active)
                active = True
                continue

            if re.match(r"^\s*/\*\s*\*CFFI-OFF\*\s*\*/\s*$", line) or re.match(r"^\s*//\s*\*CFFI-OFF\*\s*$", line):
                assert(active)
                active = False
                continue

            # Remove LIBHARP_API prefix.
            line = re.sub("^\s*LIBHARP_API\s*", "", line)

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

    assert(not active)
    return cdefs.getvalue()

def main(header_path, output_path):
    ffi = cffi.FFI()
    ffi.set_source("_harpc", None)
    ffi.cdef(read_header_file(header_path))
    ffi.emit_python_code(output_path)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print "usage: %s <path to harp.h.in> <output Python file>" % sys.argv[0]
        print "generate Python wrapper for the HARP C library using cffi (ABI level, out-of-line)"
        sys.exit(1)

    main(sys.argv[1], sys.argv[2])
