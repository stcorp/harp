# Copyright (C) 2015-2021 S[&]T, The Netherlands.
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

from collections import OrderedDict
import datetime
import glob
import numpy
import os

try:
    from cStringIO import StringIO
except ImportError:
    try:
        from StringIO import StringIO
    except ImportError:
        from io import StringIO

from harp._harpc import ffi as _ffi

__all__ = ["Error", "CLibraryError", "UnsupportedTypeError", "UnsupportedDimensionError", "NoDataError", "Variable",
           "Product", "get_encoding", "set_encoding", "version", "import_product", "import_product_metadata",
           "export_product", "concatenate", "execute_operations", "convert_unit", "to_dict"]


class Error(Exception):
    """Exception base class for all HARP Python interface errors."""
    pass


class CLibraryError(Error):
    """Exception raised when an error occurs inside the HARP C library.

    Attributes:
        errno       --  error code; if None, the error code will be retrieved from
                        the HARP C library.
        strerror    --  error message; if None, the error message will be retrieved
                        from the HARP C library.

    """
    def __init__(self, errno=None, strerror=None):
        if errno is None:
            errno = _lib.harp_errno

        if strerror is None:
            strerror = _decode_string(_ffi.string(_lib.harp_errno_to_string(errno)))

        super(CLibraryError, self).__init__(errno, strerror)
        self.errno = errno
        self.strerror = strerror

    def __str__(self):
        return self.strerror


class UnsupportedTypeError(Error):
    """Exception raised when unsupported types are encountered, either on the Python
    or on the C side of the interface.

    """
    pass


class UnsupportedDimensionError(Error):
    """Exception raised when unsupported dimensions are encountered, either on the
    Python or on the C side of the interface.

    """
    pass


class NoDataError(Error):
    """Exception raised when the product returned from an import contains no variables,
    or variables without data.

    """
    def __init__(self):
        super(NoDataError, self).__init__("product contains no variables, or variables without data")


class Variable(object):
    """Python representation of a HARP variable.

    A variable consists of data (either a scalar or NumPy array), a list of
    dimension types that describe the dimensions of the data, and a number of
    optional attributes: physical unit, minimum valid value, maximum valid
    value, human-readable description, and enumeration name list.

    """
    def __init__(self, data, dimension=[], unit=None, valid_min=None, valid_max=None, description=None, enum=None):
        self.data = data
        self.dimension = dimension

        if unit is not None:
            self.unit = unit
        if valid_min is not None:
            self.valid_min = valid_min
        if valid_max is not None:
            self.valid_max = valid_max
        if description is not None:
            self.description = description
        if enum is not None:
            self.enum = enum

    def __repr__(self):
        if not self.dimension:
            return "<Variable type=%s>" % _format_data_type(self.data)

        return "<Variable type=%s dimension=%s>" % (_format_data_type(self.data),
                                                    _format_dimensions(self.dimension, self.data))

    def __str__(self):
        stream = StringIO()

        print("type =", _format_data_type(self.data), file=stream)

        if self.dimension:
            print("dimension =", _format_dimensions(self.dimension, self.data), file=stream)

        try:
            unit = self.unit
        except AttributeError:
            pass
        else:
            if unit is not None:
                print("unit = %r" % unit, file=stream)

        try:
            valid_min = self.valid_min
        except AttributeError:
            pass
        else:
            if valid_min is not None:
                print("valid_min = %r" % valid_min, file=stream)

        try:
            valid_max = self.valid_max
        except AttributeError:
            pass
        else:
            if valid_max is not None:
                print("valid_max = %r" % valid_max, file=stream)

        try:
            description = self.description
        except AttributeError:
            pass
        else:
            if description:
                print("description = %r" % description, file=stream)

        try:
            enum = self.enum
        except AttributeError:
            pass
        else:
            if enum:
                print("enum = %r" % enum, file=stream)

        if self.data is not None:
            if not isinstance(self.data, numpy.ndarray) and not numpy.isscalar(self.data):
                print("data = <invalid>", file=stream)
            elif numpy.isscalar(self.data):
                print("data = %r" % self.data, file=stream)
            elif not self.dimension and self.data.size == 1:
                print("data = %r" % self.data.flat[0], file=stream)
            elif self.data.size == 0:
                print("data = <empty>", file=stream)
            else:
                print("data =", file=stream)
                print(str(self.data), file=stream)

        return stream.getvalue()


class Product(object):
    """Python representation of a HARP product.

    A product consists of product attributes and variables. Any attribute of a
    Product instance of which the name does not start with an underscore is either
    a variable or a product attribute. Product attribute names are reserved and
    cannot be used for variables.

    The list of names reserved for product attributes is:
        source_product  --  Name of the original product this product is derived
                            from.
        history         --  New-line separated list of invocations of HARP command
                            line tools that have been performed on the product.
    """
    # Product attribute names. All attribute names of this class that do not start with an underscore are assumed to be
    # HARP variable names, except for the reserved names listed below.
    _reserved_names = set(("source_product", "history"))

    def __init__(self, source_product="", history=""):
        if source_product:
            self.source_product = source_product
        if history:
            self.history = history
        self._variable_dict = OrderedDict()

    def _is_reserved_name(self, name):
        return name.startswith("_") or name in Product._reserved_names

    def _verify_key(self, key):
        if not isinstance(key, str):
            # The statement obj.__class__.__name__ works for both new-style and old-style classes.
            raise TypeError("key must be str, not %r" % key.__class__.__name__)

        if self._is_reserved_name(key):
            raise KeyError(key)

    def __setattr__(self, name, value):
        super(Product, self).__setattr__(name, value)
        if not self._is_reserved_name(name):
            self._variable_dict[str(name)] = value

    def __delattr__(self, name):
        super(Product, self).__delattr__(name)
        if not self._is_reserved_name(name):
            del self._variable_dict[str(name)]

    def __getitem__(self, key):
        self._verify_key(key)
        try:
            return getattr(self, key)
        except AttributeError:
            raise KeyError(key)

    def __setitem__(self, key, value):
        self._verify_key(key)
        setattr(self, key, value)

    def __delitem__(self, key):
        self._verify_key(key)
        try:
            delattr(self, key)
        except AttributeError:
            raise KeyError(key)

    def __len__(self):
        return len(self._variable_dict)

    def __iter__(self):
        return iter(self._variable_dict)

    def __reversed__(self):
        return reversed(self._variable_dict)

    def __contains__(self, name):
        return name in self._variable_dict

    def __repr__(self):
        return "<Product variables=%r>" % self._variable_dict.keys()

    def __str__(self):
        stream = StringIO()

        # Attributes.
        has_attributes = False

        try:
            source_product = self.source_product
        except AttributeError:
            pass
        else:
            if source_product:
                print("source product = %r" % source_product, file=stream)
                has_attributes = True

        try:
            history = self.history
        except AttributeError:
            pass
        else:
            if history:
                print("history = %r" % history, file=stream)
                has_attributes = True

        # Variables.
        if not self._variable_dict:
            return stream.getvalue()

        if has_attributes:
            stream.write("\n")

        for name, variable in _dict_iteritems(self._variable_dict):
            if not isinstance(variable, Variable):
                print("<non-compliant variable %r>" % name, file=stream)
                continue

            if not isinstance(variable.data, numpy.ndarray) and not numpy.isscalar(variable.data):
                print("<non-compliant variable %r>" % name, file=stream)
                continue

            if isinstance(variable.data, numpy.ndarray) and variable.data.size == 0:
                print("<empty variable %r>" % name, file=stream)
                continue

            # Data type and variable name.
            stream.write(_format_data_type(variable.data) + " " + name)

            # Dimensions.
            if variable.dimension:
                stream.write(" " + _format_dimensions(variable.dimension, variable.data))

            # Unit.
            try:
                unit = variable.unit
            except AttributeError:
                pass
            else:
                if unit is not None:
                    stream.write(" [%s]" % unit)

            stream.write("\n")

        return stream.getvalue()


def _get_c_library_filename():
    """Return the filename of the HARP shared library depending on the current
    platform.

    """
    from platform import system as _system

    if _system() == "Windows":
        return "harp.dll"

    if _system() == "Darwin":
        library_name = "libharp.dylib"
    else:
        library_name = "libharp.so"

    import os.path

    # check for library file in the parent directory (for pyinstaller bundles)
    library_path = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", library_name))
    if os.path.exists(library_path):
        return library_path

    # assume the library to be in the parent library directory
    library_path = os.path.normpath(os.path.join(os.path.dirname(__file__), "../../..", library_name))
    if not os.path.exists(library_path):
        # on RHEL the python path uses lib64, but the library might have gotten installed into lib
        alt_library_path = os.path.normpath(os.path.join(os.path.dirname(__file__), "../../../../lib", library_name))
        if os.path.exists(alt_library_path):
            return alt_library_path
    return library_path


def _get_filesystem_encoding():
    """Return the encoding used by the filesystem."""
    from sys import getdefaultencoding as _getdefaultencoding, getfilesystemencoding as _getfilesystemencoding

    encoding = _getfilesystemencoding()
    if encoding is None:
        encoding = _getdefaultencoding()

    return encoding


def _init():
    """Initialize the HARP Python interface."""
    global _lib, _encoding, _py_dimension_type, _c_dimension_type, _py_data_type, _c_data_type_name
    from platform import system as _system

    # Initialize the HARP C library
    clib = _get_c_library_filename()
    _lib = _ffi.dlopen(clib)

    if os.getenv('CODA_DEFINITION') is None:
        # Set coda definition path relative to C library
        relpath = "../share/coda/definitions"
        if _system() == "Windows":
            _lib.harp_set_coda_definition_path_conditional(_encode_path(os.path.basename(clib)), _ffi.NULL,
                                                           _encode_path(relpath))
        else:
            _lib.harp_set_coda_definition_path_conditional(_encode_path(os.path.basename(clib)),
                                                           _encode_path(os.path.dirname(clib)),
                                                           _encode_path(relpath))

    if os.getenv('UDUNITS2_XML_PATH') is None:
        # Set udunits2 xml path relative to C library
        relpath = "../share/harp/udunits2.xml"
        if _system() == "Windows":
            _lib.harp_set_udunits2_xml_path_conditional(_encode_path(os.path.basename(clib)), _ffi.NULL,
                                                        _encode_path(relpath))
        else:
            _lib.harp_set_udunits2_xml_path_conditional(_encode_path(os.path.basename(clib)),
                                                        _encode_path(os.path.dirname(clib)),
                                                        _encode_path(relpath))

    if _lib.harp_init() != 0:
        raise CLibraryError()

    # Set default encoding.
    _encoding = "ascii"

    # Initialize various look-up tables used throughout the HARP Python interface (i.e. this module).
    _py_dimension_type = \
        {
            _lib.harp_dimension_independent: None,
            _lib.harp_dimension_time: "time",
            _lib.harp_dimension_latitude: "latitude",
            _lib.harp_dimension_longitude: "longitude",
            _lib.harp_dimension_vertical: "vertical",
            _lib.harp_dimension_spectral: "spectral"
        }

    _c_dimension_type = \
        {
            None: _lib.harp_dimension_independent,
            "time": _lib.harp_dimension_time,
            "latitude": _lib.harp_dimension_latitude,
            "longitude": _lib.harp_dimension_longitude,
            "vertical": _lib.harp_dimension_vertical,
            "spectral": _lib.harp_dimension_spectral
        }

    _py_data_type = \
        {
            _lib.harp_type_int8: numpy.int8,
            _lib.harp_type_int16: numpy.int16,
            _lib.harp_type_int32: numpy.int32,
            _lib.harp_type_float: numpy.float32,
            _lib.harp_type_double: numpy.float64,
            _lib.harp_type_string: numpy.object_
        }

    _c_data_type_name = \
        {
            _lib.harp_type_int8: "byte",
            _lib.harp_type_int16: "int",
            _lib.harp_type_int32: "long",
            _lib.harp_type_float: "float",
            _lib.harp_type_double: "double",
            _lib.harp_type_string: "string"
        }


def _dict_iteritems(dictionary):
    """Get an iterator or view on the items of the specified dictionary.

    This method is Python 2 and Python 3 compatible.
    """
    try:
        return dictionary.iteritems()
    except AttributeError:
        return dictionary.items()


def _get_py_dimension_type(dimension_type):
    """Return the dimension name corresponding to the specified C dimension type
    code.

    """
    try:
        return _py_dimension_type[dimension_type]
    except KeyError:
        raise UnsupportedDimensionError("unsupported C dimension type code '%d'" % dimension_type)


def _get_c_dimension_type(dimension_type):
    """Return the C dimension type code corresponding to the specified dimension
    name.

    """
    try:
        return _c_dimension_type[dimension_type]
    except KeyError:
        raise UnsupportedDimensionError("unsupported dimension %r" % dimension_type)


def _get_py_data_type(data_type):
    """Return the Python type corresponding to the specified C data type code."""
    try:
        return _py_data_type[data_type]
    except KeyError:
        raise UnsupportedTypeError("unsupported C data type code '%d'" % data_type)


def _get_c_data_type(value):
    """Return the C data type code corresponding to the specified variable data
    value.

    """
    if isinstance(value, (numpy.ndarray, numpy.generic)):
        # For NumPy ndarrays and scalars, determine the smallest HARP C data type that can safely contain elements of
        # the ndarray or scalar dtype.
        if numpy.issubdtype(value.dtype, numpy.object_):
            # NumPy object arrays are only used to contain variable length strings or byte strings.
            if all(isinstance(x, str) for x in value.flat):
                return _lib.harp_type_string
            elif all(isinstance(x, bytes) for x in value.flat):
                return _lib.harp_type_string
            else:
                raise UnsupportedTypeError("elements of a NumPy object array must be all str or all bytes")
        elif numpy.issubdtype(value.dtype, numpy.str_) or numpy.issubdtype(value.dtype, numpy.bytes_):
            return _lib.harp_type_string
        elif numpy.can_cast(value.dtype, numpy.int8):
            return _lib.harp_type_int8
        elif numpy.can_cast(value.dtype, numpy.int16):
            return _lib.harp_type_int16
        elif numpy.can_cast(value.dtype, numpy.int32):
            return _lib.harp_type_int32
        elif numpy.can_cast(value.dtype, numpy.float32):
            return _lib.harp_type_float
        elif numpy.can_cast(value.dtype, numpy.float64):
            return _lib.harp_type_double
        else:
            raise UnsupportedTypeError("unsupported NumPy dtype '%s'" % value.dtype)
    elif numpy.isscalar(value):
        if isinstance(value, (str, bytes)):
            return _lib.harp_type_string
        elif numpy.can_cast(value, numpy.int8):
            return _lib.harp_type_int8
        elif numpy.can_cast(value, numpy.int16):
            return _lib.harp_type_int16
        elif numpy.can_cast(value, numpy.int32):
            return _lib.harp_type_int32
        elif numpy.can_cast(value, numpy.float32):
            return _lib.harp_type_float
        elif numpy.can_cast(value, numpy.float64):
            return _lib.harp_type_double
        else:
            raise UnsupportedTypeError("unsupported type %r" % value.__class__.__name__)
    else:
        raise UnsupportedTypeError("unsupported type %r" % value.__class__.__name__)


def _get_c_data_type_name(data_type):
    """Return the canonical name for the specified C data type code."""
    try:
        return _c_data_type_name[data_type]
    except KeyError:
        raise UnsupportedTypeError("unsupported C data type code '%d'" % data_type)


def _c_can_cast(c_data_type_src, c_data_type_dst):
    """Returns True if the source C data type can be cast to the destination C data
    type while preserving values.

    """
    if c_data_type_dst == _lib.harp_type_int8:
        return (c_data_type_src == _lib.harp_type_int8)
    elif c_data_type_dst == _lib.harp_type_int16:
        return (c_data_type_src == _lib.harp_type_int8 or
                c_data_type_src == _lib.harp_type_int16)
    elif c_data_type_dst == _lib.harp_type_int32:
        return (c_data_type_src == _lib.harp_type_int8 or
                c_data_type_src == _lib.harp_type_int16 or
                c_data_type_src == _lib.harp_type_int32)
    elif c_data_type_dst == _lib.harp_type_float:
        return (c_data_type_src == _lib.harp_type_int8 or
                c_data_type_src == _lib.harp_type_int16 or
                c_data_type_src == _lib.harp_type_float or
                c_data_type_src == _lib.harp_type_double)
    elif c_data_type_dst == _lib.harp_type_double:
        return (c_data_type_src == _lib.harp_type_int8 or
                c_data_type_src == _lib.harp_type_int16 or
                c_data_type_src == _lib.harp_type_int32 or
                c_data_type_src == _lib.harp_type_float or
                c_data_type_src == _lib.harp_type_double)
    elif c_data_type_dst == _lib.harp_type_string:
        return (c_data_type_src == _lib.harp_type_string)
    else:
        return False


def _encode_string_with_encoding(string, encoding="utf-8"):
    """Encode a unicode string using the specified encoding.

    By default, use the "surrogateescape" error handler to deal with encoding
    errors. This error handler ensures that invalid bytes encountered during
    decoding are converted to the same bytes during encoding, by decoding them
    to a special range of unicode code points.

    The "surrogateescape" error handler is available since Python 3.1. For earlier
    versions of Python 3, the "strict" error handler is used instead.

    """
    try:
        try:
            return string.encode(encoding, "surrogateescape")
        except LookupError:
            # Either the encoding or the error handler is not supported; fall-through to the next try-block.
            pass

        try:
            return string.encode(encoding)
        except LookupError:
            # Here it is certain that the encoding is not supported.
            raise Error("unknown encoding '%s'" % encoding)
    except UnicodeEncodeError:
        raise Error("cannot encode '%s' using encoding '%s'" % (string, encoding))


def _decode_string_with_encoding(string, encoding="utf-8"):
    """Decode a byte string using the specified encoding.

    By default, use the "surrogateescape" error handler to deal with encoding
    errors. This error handler ensures that invalid bytes encountered during
    decoding are converted to the same bytes during encoding, by decoding them
    to a special range of unicode code points.

    The "surrogateescape" error handler is available since Python 3.1. For earlier
    versions of Python 3, the "strict" error handler is used instead. This may cause
    decoding errors if the input byte string contains bytes that cannot be decoded
    using the specified encoding. Since most HARP products use ASCII strings
    exclusively, it is unlikely this will occur often in practice.

    """
    try:
        try:
            return string.decode(encoding, "surrogateescape")
        except LookupError:
            # Either the encoding or the error handler is not supported; fall-through to the next try-block.
            pass

        try:
            return string.decode(encoding)
        except LookupError:
            # Here it is certain that the encoding is not supported.
            raise Error("unknown encoding '%s'" % encoding)
    except UnicodeEncodeError:
        raise Error("cannot decode '%s' using encoding '%s'" % (string, encoding))


def _encode_path(path):
    """Encode the input unicode path using the filesystem encoding.

    On Python 2, this method returns the specified path unmodified.

    """
    if isinstance(path, bytes):
        # This branch will be taken for instances of class str on Python 2 (since this is an alias for class bytes), and
        # on Python 3 for instances of class bytes.
        return path
    elif isinstance(path, str):
        # This branch will only be taken for instances of class str on Python 3. On Python 2 such instances will take
        # the branch above.
        return _encode_string_with_encoding(path, _get_filesystem_encoding())
    else:
        raise TypeError("path must be bytes or str, not %r" % path.__class__.__name__)


def _encode_string(string):
    """Encode the input unicode string using the package default encoding.

    On Python 2, this method returns the specified string unmodified.

    """
    if isinstance(string, bytes):
        # This branch will be taken for instances of class str on Python 2 (since this is an alias for class bytes), and
        # on Python 3 for instances of class bytes.
        return string
    elif isinstance(string, str):
        # This branch will only be taken for instances of class str on Python 3. On Python 2 such instances will take
        # the branch above.
        return _encode_string_with_encoding(string, get_encoding())
    else:
        raise TypeError("string must be bytes or str, not %r" % string.__class__.__name__)


def _decode_string(string):
    """Decode the input byte string using the package default encoding.

    On Python 2, this method returns the specified byte string unmodified.

    """
    if isinstance(string, str):
        # This branch will be taken for instances of class str on Python 2 and Python 3.
        return string
    elif isinstance(string, bytes):
        # This branch will only be taken for instances of class bytes on Python 3. On Python 2 such instances will take
        # the branch above.
        return _decode_string_with_encoding(string, get_encoding())
    else:
        raise TypeError("string must be bytes or str, not %r" % string.__class__.__name__)


def _format_data_type(data):
    """Return the string representation of the C data type that would be used to
    store the specified data, or "<invalid>" if the specified data is of an
    unsupported type.

    """
    try:
        return _get_c_data_type_name(_get_c_data_type(data))
    except UnsupportedTypeError:
        return "<invalid>"


def _format_dimensions(dimension, data):
    """Construct a formatted string from the specified dimensions and data that
    provides information about dimension types and lengths, or "<invalid>" if this
    information cannot be determined.

    """
    if not isinstance(data, numpy.ndarray) or data.ndim != len(dimension):
        return "{<invalid>}"

    stream = StringIO()

    stream.write("{")
    for i in range(data.ndim):
        if dimension[i]:
            stream.write(dimension[i] + "=")
        stream.write(str(data.shape[i]))

        if (i + 1) < data.ndim:
            stream.write(", ")
    stream.write("}")

    return stream.getvalue()


def _import_scalar(c_data_type, c_data):
    if c_data_type == _lib.harp_type_int8:
        return c_data.int8_data
    elif c_data_type == _lib.harp_type_int16:
        return c_data.int16_data
    elif c_data_type == _lib.harp_type_int32:
        return c_data.int32_data
    elif c_data_type == _lib.harp_type_float:
        return c_data.float_data
    elif c_data_type == _lib.harp_type_double:
        return c_data.double_data

    raise UnsupportedTypeError("unsupported C data type code '%d'" % c_data_type)


def _import_array(c_data_type, c_num_elements, c_data):
    if c_data_type == _lib.harp_type_string:
        data = numpy.empty((c_num_elements,), dtype=numpy.object)
        for i in range(c_num_elements):
            # NB. The _ffi.string() method returns a copy of the C string.
            data[i] = _decode_string(_ffi.string(c_data.string_data[i]))
        return data

    # NB. The _ffi.buffer() method, as well as the numpy.frombuffer() method, provide a view on the C array; neither
    # method incurs a copy.
    c_data_buffer = _ffi.buffer(c_data.ptr, c_num_elements * _lib.harp_get_size_for_type(c_data_type))
    return numpy.copy(numpy.frombuffer(c_data_buffer, dtype=_get_py_data_type(c_data_type)))


def _import_variable(c_variable):
    # Import variable data.
    data = _import_array(c_variable.data_type, c_variable.num_elements, c_variable.data)

    num_dimensions = c_variable.num_dimensions
    if num_dimensions == 0:
        variable = Variable(numpy.asscalar(data))
    else:
        data = data.reshape([c_variable.dimension[i] for i in range(num_dimensions)])
        dimension = [_get_py_dimension_type(c_variable.dimension_type[i]) for i in range(num_dimensions)]
        variable = Variable(data, dimension)

    # Import variable attributes.
    if c_variable.unit != _ffi.NULL:
        variable.unit = _decode_string(_ffi.string(c_variable.unit))

    if c_variable.data_type != _lib.harp_type_string:
        variable.valid_min = _import_scalar(c_variable.data_type, c_variable.valid_min)
        variable.valid_max = _import_scalar(c_variable.data_type, c_variable.valid_max)

    if c_variable.description:
        variable.description = _decode_string(_ffi.string(c_variable.description))

    num_enum_values = c_variable.num_enum_values
    if num_enum_values > 0 and c_variable.enum_name != _ffi.NULL:
        variable.enum = [_decode_string(_ffi.string(c_variable.enum_name[i])) for i in range(num_enum_values)]

    return variable


def _import_product(c_product):
    product = Product()

    # Import product attributes.
    if c_product.source_product:
        product.source_product = _decode_string(_ffi.string(c_product.source_product))

    if c_product.history:
        product.history = _decode_string(_ffi.string(c_product.history))

    # Import variables.
    for i in range(c_product.num_variables):
        c_variable_ptr = c_product.variable[i]
        variable = _import_variable(c_variable_ptr[0])
        setattr(product, _decode_string(_ffi.string(c_variable_ptr[0].name)), variable)

    return product


def _import_product_metadata(c_metadata):
    metadata = OrderedDict()

    metadata['filename'] = _decode_string(_ffi.string(c_metadata.filename))

    if numpy.isinf(c_metadata.datetime_start):
        metadata['datetime_start'] = datetime.datetime.min
    else:
        metadata['datetime_start'] = datetime.datetime(2000, 1, 1) + datetime.timedelta(days=c_metadata.datetime_start)
    if numpy.isinf(c_metadata.datetime_stop):
        metadata['datetime_stop'] = datetime.datetime.max
    else:
        metadata['datetime_stop'] = datetime.datetime(2000, 1, 1) + datetime.timedelta(days=c_metadata.datetime_stop)

    metadata['time'] = c_metadata.dimension[0]
    metadata['latitude'] = c_metadata.dimension[1]
    metadata['longitude'] = c_metadata.dimension[2]
    metadata['vertical'] = c_metadata.dimension[3]
    metadata['spectral'] = c_metadata.dimension[4]

    metadata['format'] = _decode_string(_ffi.string(c_metadata.format))

    metadata['source_product'] = _decode_string(_ffi.string(c_metadata.source_product))

    if c_metadata.history:
        metadata['history'] = _decode_string(_ffi.string(c_metadata.history))

    return metadata


def _export_scalar(data, c_data_type, c_data):
    if c_data_type == _lib.harp_type_int8:
        c_data.int8_data = data
    elif c_data_type == _lib.harp_type_int16:
        c_data.int16_data = data
    elif c_data_type == _lib.harp_type_int32:
        c_data.int32_data = data
    elif c_data_type == _lib.harp_type_float:
        c_data.float_data = data
    elif c_data_type == _lib.harp_type_double:
        c_data.double_data = data
    else:
        raise UnsupportedTypeError("unsupported C data type code '%d'" % c_data_type)


def _export_array(data, c_variable):
    if c_variable.data_type != _lib.harp_type_string:
        # NB. The _ffi.buffer() method as well as the numpy.frombuffer() method provide a view on the C array; neither
        # method incurs a copy. The numpy.copyto() method also works if the source array is a scalar, i.e. not an
        # instance of numpy.ndarray.
        size = c_variable.num_elements * _lib.harp_get_size_for_type(c_variable.data_type)
        shape = data.shape if isinstance(data, numpy.ndarray) else ()

        c_data_buffer = _ffi.buffer(c_variable.data.ptr, size)
        c_data = numpy.reshape(numpy.frombuffer(c_data_buffer, dtype=_get_py_data_type(c_variable.data_type)), shape)
        numpy.copyto(c_data, data, casting="safe")
    elif isinstance(data, numpy.ndarray):
        for index, element in enumerate(data.flat):
            if _lib.harp_variable_set_string_data_element(c_variable, index, _encode_string(element)) != 0:
                raise CLibraryError()
    else:
        assert(c_variable.num_elements == 1)
        if _lib.harp_variable_set_string_data_element(c_variable, 0, _encode_string(data)) != 0:
            raise CLibraryError()


def _export_variable(name, variable, c_product):
    data = getattr(variable, "data", None)
    if data is None:
        raise Error("no data or data is None")

    # Check dimensions
    dimension = getattr(variable, "dimension", [])
    if not dimension and isinstance(data, numpy.ndarray) and data.size != 1:
        raise Error("dimensions missing or incomplete")

    if dimension and (not isinstance(data, numpy.ndarray) or data.ndim != len(dimension)):
        raise Error("dimensions incorrect")

    # Determine C data type.
    c_data_type = _get_c_data_type(data)
    if len(dimension) == 0:
        # Allow valid_min/valid_max to influence data type as well
        try:
            min_data_type = _get_c_data_type(variable.valid_min)
            if min_data_type != c_data_type:
                c_data_type = min_data_type
        except:
            pass
        try:
            max_data_type = _get_c_data_type(variable.valid_max)
            if max_data_type != c_data_type:
                c_data_type = max_data_type
        except:
            pass

    # Encode variable name.
    c_name = _encode_string(name)

    # Determine C dimension types and lengths.
    c_num_dimensions = len(dimension)
    c_dimension_type = [_get_c_dimension_type(dimension_name) for dimension_name in dimension]
    c_dimension = _ffi.NULL if not dimension else data.shape

    # Create C variable of the proper size.
    c_variable_ptr = _ffi.new("harp_variable **")
    if _lib.harp_variable_new(c_name, c_data_type, c_num_dimensions, c_dimension_type, c_dimension,
                              c_variable_ptr) != 0:
        raise CLibraryError()

    # Add C variable to C product.
    if _lib.harp_product_add_variable(c_product, c_variable_ptr[0]) != 0:
        _lib.harp_variable_delete(c_variable_ptr[0])
        raise CLibraryError()

    # The C variable has been successfully added to the C product. Therefore, the memory management of the C variable
    # is tied to the life time of the C product. If an error occurs, the memory occupied by the C variable will be
    # freed along with the C product.
    c_variable = c_variable_ptr[0]

    # Copy data into the C variable.
    _export_array(data, c_variable)

    # Variable attributes.
    if c_data_type != _lib.harp_type_string:
        try:
            valid_min = variable.valid_min
        except AttributeError:
            pass
        else:
            if isinstance(valid_min, numpy.ndarray):
                if valid_min.size == 1:
                    valid_min = valid_min.flat[0]
                else:
                    raise Error("valid_min attribute should be scalar")

            c_data_type_valid_min = _get_c_data_type(valid_min)
            if _c_can_cast(c_data_type_valid_min, c_data_type):
                _export_scalar(valid_min, c_data_type, c_variable.valid_min)
            else:
                raise Error("type '%s' of valid_min attribute incompatible with type '%s' of data"
                            % (_get_c_data_type_name(c_data_type_valid_min), _get_c_data_type_name(c_data_type)))

        try:
            valid_max = variable.valid_max
        except AttributeError:
            pass
        else:
            if isinstance(valid_max, numpy.ndarray):
                if valid_max.size == 1:
                    valid_max = valid_max.flat[0]
                else:
                    raise Error("valid_max attribute should be scalar")

            c_data_type_valid_max = _get_c_data_type(valid_max)
            if _c_can_cast(c_data_type_valid_max, c_data_type):
                _export_scalar(valid_max, c_data_type, c_variable.valid_max)
            else:
                raise Error("type '%s' of valid_max attribute incompatible with type '%s' of data"
                            % (_get_c_data_type_name(c_data_type_valid_max), _get_c_data_type_name(c_data_type)))

    try:
        unit = variable.unit
    except AttributeError:
        pass
    else:
        if unit is not None and _lib.harp_variable_set_unit(c_variable, _encode_string(unit)) != 0:
            raise CLibraryError()

    try:
        description = variable.description
    except AttributeError:
        pass
    else:
        if description and _lib.harp_variable_set_description(c_variable, _encode_string(description)) != 0:
            raise CLibraryError()

    try:
        enum = variable.enum
    except AttributeError:
        pass
    else:
        if enum and _lib.harp_variable_set_enumeration_values(c_variable, len(enum),
                                                              [_ffi.new("char[]",
                                                               _encode_string(name)) for name in enum]) != 0:
            raise CLibraryError()


def _export_product(product, c_product):
    # Export product attributes.
    try:
        source_product = product.source_product
    except AttributeError:
        pass
    else:
        if source_product and _lib.harp_product_set_source_product(c_product, _encode_string(source_product)) != 0:
            raise CLibraryError()

    try:
        history = product.history
    except AttributeError:
        pass
    else:
        if history and _lib.harp_product_set_history(c_product, _encode_string(history)) != 0:
            raise CLibraryError()

    # Export variables.
    for name in product:
        try:
            _export_variable(name, product[name], c_product)
        except Error as _error:
            raise Error("variable '%r' could not be exported (%s)" % (name, str(_error)))


def _update_history(product, command):
    line = datetime.datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ")
    line += " [harp-%s] " % (version())
    line += command
    try:
        product.history += "\n" + line
    except AttributeError:
        product.history = line


def get_encoding():
    """Return the encoding used to convert between unicode strings and C strings
    (only relevant when using Python 3).

    """
    return _encoding


def set_encoding(encoding):
    """Set the encoding used to convert between unicode strings and C strings
    (only relevant when using Python 3).

    """
    global _encoding

    _encoding = encoding


def version():
    """Return the version of the HARP C library."""
    return _decode_string(_ffi.string(_lib.libharp_version))


def to_dict(product):
    """Convert a Product to an OrderedDict.

    The OrderedDict representation provides direct access to the data associated
    with each variable. All product attributes and all variable attributes
    except the unit attribute are discarded as part of the conversion.

    The unit attribute of a variable is represented by adding a scalar variable
    of type string with the name of the corresponding variable suffixed with
    '_unit' as name and the unit as value.

    Arguments:
    product -- Product to convert.

    """
    if not isinstance(product, Product):
        raise TypeError("product must be Product, not %r" % product.__class__.__name__)

    dictionary = OrderedDict()

    for name in product:
        variable = product[name]

        try:
            dictionary[name] = variable.data
            if variable.unit is not None:
                dictionary[name + "_unit"] = variable.unit
        except AttributeError:
            pass

    return dictionary


def import_product(filename, operations="", options="", reduce_operations="", post_operations=""):
    """Import a product from a file.

    This will first try to import the file as an HDF4, HDF5, or netCDF file that
    complies to the HARP Data Format. If the file is not stored using the HARP
    format then it will try to import it using one of the available ingestion
    modules.

    If the filename argument is a list of filenames, a globbing (glob.glob())
    pattern, or a list of globbing patterns then the harp.import_product() function
    will be called on each individual matching file. All imported products will then
    be appended into a single merged product and that merged product will be returned.

    Arguments:
    filename -- Filename, file pattern, or list of filenames/patterns of the product(s)
                to import
    operations -- Actions to apply as part of the import; should be specified as a
                  semi-colon separated string of operations;
                  in case a list of products is ingested these operations will be
                  performed on each product individually before the data is merged.
    options -- Ingestion module specific options; should be specified as a semi-
               colon separated string of key=value pairs; only used if a file is not
               in HARP format.
    reduce_operations -- Actions to apply after each append; should be specified as a
                       semi-colon separated string of operations;
                       these operations will only be applied if the filename argument is
                       a file pattern or a list of filenames/patterns;
                       this advanced option allows for memory efficient application
                       of time reduction operations (such as bin()) that would
                       normally be provided as part of post_operations.
    post_operations -- Actions to apply after the list of products is merged; should be
                       specified as a semi-colon separated string of operations;
                       these operations will only be applied if the filename argument is
                       a file pattern or a list of filenames/patterns.
    """
    filenames = None
    if not (isinstance(filename, bytes) or isinstance(filename, str)):
        # Assume this is a list of filenames or patterns
        filenames = []
        for file in filename:
            if '*' in file or '?' in file:
                # This is a globbing pattern
                filenames.extend(sorted(glob.glob(file)))
            else:
                filenames.append(file)
    elif '*' in filename or '?' in filename:
        # This is a globbing pattern
        filenames = sorted(glob.glob(filename))

    if filenames is not None:
        if len(filenames) == 0:
            raise Error("no files matching '%s'" % (filename))
        # Return the merged concatenation of all products
        merged_product_ptr = None
        try:
            for file in filenames:
                c_product_ptr = _ffi.new("harp_product **")

                # Import the product as a C product.
                if _lib.harp_import(_encode_path(file), _encode_string(operations), _encode_string(options),
                                    c_product_ptr) != 0:
                    raise CLibraryError()
                if _lib.harp_product_is_empty(c_product_ptr[0]) == 1:
                    _lib.harp_product_delete(c_product_ptr[0])
                else:
                    if merged_product_ptr is None:
                        merged_product_ptr = c_product_ptr
                        # if this remains the only product, make sure it still looks like it was the result of a merge
                        if _lib.harp_product_append(merged_product_ptr[0], _ffi.NULL) != 0:
                            raise CLibraryError()
                    else:
                        try:
                            if _lib.harp_product_append(merged_product_ptr[0], c_product_ptr[0]) != 0:
                                raise CLibraryError()
                        finally:
                            _lib.harp_product_delete(c_product_ptr[0])
                    if reduce_operations:
                        # perform reduction operations on the partially merged product after each append
                        if _lib.harp_product_execute_operations(merged_product_ptr[0],
                                                                _encode_string(reduce_operations)) != 0:
                            raise CLibraryError()
        except:
            if merged_product_ptr is not None:
                _lib.harp_product_delete(merged_product_ptr[0])
            raise

        if merged_product_ptr is None:
            raise NoDataError()

        try:
            if post_operations:
                if _lib.harp_product_execute_operations(merged_product_ptr[0], _encode_string(post_operations)) != 0:
                    raise CLibraryError()
            # Convert the merged C product into its Python representation.
            product = _import_product(merged_product_ptr[0])
        finally:
            _lib.harp_product_delete(merged_product_ptr[0])

        if operations or options or post_operations:
            # Update history
            command = "harp.import_product('{0}'".format(filename)
            if operations:
                command += ",operations='{0}'".format(operations)
            if options:
                command += ",options='{0}'".format(options)
            if reduce_operations:
                command += ",reduce_operations='{0}'".format(reduce_operations)
            if post_operations:
                command += ",post_operations='{0}'".format(post_operations)
            command += ")"
            _update_history(product, command)

        return product

    c_product_ptr = _ffi.new("harp_product **")

    # Import the product as a C product.
    if _lib.harp_import(_encode_path(filename), _encode_string(operations), _encode_string(options),
                        c_product_ptr) != 0:
        raise CLibraryError()

    try:
        # Raise an exception if the imported C product contains no variables, or variables without data.
        if _lib.harp_product_is_empty(c_product_ptr[0]) == 1:
            raise NoDataError()

        # Convert the C product into its Python representation.
        product = _import_product(c_product_ptr[0])

        if operations or options:
            # Update history
            command = "harp.import_product('{0}'".format(filename)
            if operations:
                command += ",operations='{0}'".format(operations)
            if options:
                command += ",options='{0}'".format(options)
            command += ")"
            _update_history(product, command)

        return product

    finally:
        _lib.harp_product_delete(c_product_ptr[0])


def import_product_metadata(filename, options=""):
    """Import specific metadata from a single file.

    This will try to extract the following information from a file.
    - datetime_start
    - datetime_stop
    - dimension lengths for time, latitude, longitude, vertical, and spectral
    - source_product

    If the file is not stored using the HARP format then it will try to import
    the metadata using one of the available ingestion modules.

    Arguments:
    filename -- Filename of the product from which to extract the metadata
    options -- Ingestion module specific options; should be specified as a semi-
               colon separated string of key=value pairs; only used if a file is not
               in HARP format.
    """
    c_metadata_ptr = _ffi.new("harp_product_metadata **")

    # Import the product as a C product.
    if _lib.harp_import_product_metadata(_encode_path(filename), _encode_string(options), c_metadata_ptr) != 0:
        raise CLibraryError()

    try:
        # Convert the C metadata into its Python representation.
        return _import_product_metadata(c_metadata_ptr[0])
    finally:
        _lib.harp_product_metadata_delete(c_metadata_ptr[0])


def export_product(product, filename, file_format="netcdf", operations="", hdf5_compression=0):
    """Export a HARP compliant product.

    Arguments:
    product          -- Product to export.
    filename         -- Filename of the exported product.
    file_format      -- File format to use; one of 'netcdf', 'hdf4', or 'hdf5'.
    operations       -- Actions to apply as part of the export; should be specified as a
                        semi-colon separated string of operations.
    hdf5_compression -- Compression level when exporting to hdf5 (0=disabled, 1=low, ..., 9=high).

    """
    if not isinstance(product, Product):
        raise TypeError("product must be Product, not %r" % product.__class__.__name__)

    if operations:
        # Update history (but only if the export modifies the product)
        command = "harp.export_product('{0}', operations='{1}')".format(filename, operations)
        _update_history(product, command)

    # Create C product.
    c_product_ptr = _ffi.new("harp_product **")
    if _lib.harp_product_new(c_product_ptr) != 0:
        raise CLibraryError()

    try:
        # Convert the Python product to its C representation.
        _export_product(product, c_product_ptr[0])

        if operations:
            # Apply operations to the product before export
            if _lib.harp_product_execute_operations(c_product_ptr[0], _encode_string(operations)) != 0:
                raise CLibraryError()

        # Export the C product to a file.
        if file_format == 'hdf5':
            _lib.harp_set_option_hdf5_compression(int(hdf5_compression))
        if _lib.harp_export(_encode_path(filename), _encode_string(file_format), c_product_ptr[0]) != 0:
            raise CLibraryError()

    finally:
        _lib.harp_product_delete(c_product_ptr[0])


def _get_time_length(product):
    time_length = None
    for name in product:
        variable = product[name]
        dimension = getattr(variable, "dimension", [])
        data = numpy.asarray(variable.data)
        if dimension and len(data.shape) != len(dimension):
            raise Error("dimensions incorrect")
        for i in range(len(dimension)):
            if dimension[i] == "time":
                if time_length is None:
                    time_length = data.shape[i]
                elif time_length != data.shape[i]:
                    raise Error("inconsistent dimension lengths for 'time'")
    return time_length


def _extend_variable_for_dim(variable, dim_index, new_length):
    shape = list(variable.data.shape)
    shape[dim_index] = new_length - shape[dim_index]
    filler = numpy.empty(shape, dtype=variable.data.dtype)
    filler[:] = numpy.NAN
    variable.data = numpy.concatenate([variable.data, filler], axis=dim_index)


def make_time_dependent(product):
    time_length = _get_time_length(product)
    if time_length is None:
        raise Error("product has no time dimension")
    for name in product:
        variable = product[name]
        dimension = getattr(variable, "dimension", [])
        if len(dimension) == 0 or dimension[0] != 'time':
            # add time dimension
            data = numpy.asarray(variable.data)
            data = data.reshape([1] + list(data.shape))
            variable.data = numpy.repeat(data, time_length, axis=0)
            variable.dimension = ['time'] + dimension
    return product


def concatenate(products):
    if len(products) == 0:
        raise Error("product list is empty")

    variable_names = []
    for product in products:
        for name in product:
            if name not in variable_names and name != "index":
                variable_names.append(name)
    for name in variable_names:
        for product in products:
            if name not in product:
                raise Error("not all products contain variable '%s'" % (name,))

    for product in products:
        make_time_dependent(product)
    target_product = Product()
    for name in variable_names:
        for product in products:
            source_variable = product[name]
            if name not in target_product:
                target_variable = Variable(source_variable.data, source_variable.dimension)
                if hasattr(source_variable, 'unit'):
                    target_variable.unit = source_variable.unit
                if hasattr(source_variable, 'valid_min'):
                    target_variable.valid_min = source_variable.valid_min
                if hasattr(source_variable, 'valid_max'):
                    target_variable.valid_max = source_variable.valid_max
                if hasattr(source_variable, 'description'):
                    target_variable.description = source_variable.description
                if hasattr(source_variable, 'enum'):
                    target_variable.enum = source_variable.enum
                target_product[name] = target_variable
            else:
                target_variable = target_product[name]
                if hasattr(target_variable, 'unit'):
                    if not hasattr(source_variable, 'unit') or target_variable.unit != source_variable.unit:
                        raise Error("inconsistent units in appending variable '%s'" % (name,))
                if len(target_variable.data.shape) != len(source_variable.data.shape):
                    raise Error("inconsistent number of dimensions for appending variable '%s'" % (name,))
                for i in range(len(target_variable.data.shape))[1:]:
                    if target_variable.data.shape[i] < source_variable.data.shape[i]:
                        _extend_variable_for_dim(target_variable, i, source_variable.data.shape[i])
                    if source_variable.data.shape[i] < target_variable.data.shape[i]:
                        _extend_variable_for_dim(source_variable, i, target_variable.data.shape[i])
                target_variable.data = numpy.append(target_variable.data, source_variable.data, axis=0)
    return target_product


def execute_operations(products, operations="", post_operations=""):
    if isinstance(products, Product):
        if not operations:
            return products

        # Create C product.
        c_product_ptr = _ffi.new("harp_product **")
        if _lib.harp_product_new(c_product_ptr) != 0:
            raise harp.CLibraryError()
        try:
            _export_product(products, c_product_ptr[0])
            if _lib.harp_product_execute_operations(c_product_ptr[0], _encode_string(operations)) != 0:
                raise CLibraryError()
            product = _import_product(c_product_ptr[0])
            return product
        finally:
            _lib.harp_product_delete(c_product_ptr[0])
    else:
        # Return the merged concatenation of all products
        merged_product_ptr = None
        try:
            for product in products:
                c_product_ptr = _ffi.new("harp_product **")
                if _lib.harp_product_new(c_product_ptr) != 0:
                    raise CLibraryError()
                try:
                    _export_product(product, c_product_ptr[0])
                    if _lib.harp_product_execute_operations(c_product_ptr[0], _encode_string(operations)) != 0:
                        raise CLibraryError()
                except:
                    _lib.harp_product_delete(c_product_ptr[0])
                    raise

                if _lib.harp_product_is_empty(c_product_ptr[0]) == 1:
                    _lib.harp_product_delete(c_product_ptr[0])
                elif merged_product_ptr is None:
                    merged_product_ptr = c_product_ptr
                    # if this remains the only product then make sure it still looks like it was the result of a merge
                    if _lib.harp_product_append(merged_product_ptr[0], _ffi.NULL) != 0:
                        raise CLibraryError()
                else:
                    try:
                        if _lib.harp_product_append(merged_product_ptr[0], c_product_ptr[0]) != 0:
                            raise CLibraryError()
                    finally:
                        _lib.harp_product_delete(c_product_ptr[0])
        except:
            if merged_product_ptr is not None:
                _lib.harp_product_delete(merged_product_ptr[0])
            raise

        if merged_product_ptr is None:
            raise NoDataError()

        try:
            if post_operations:
                if _lib.harp_product_execute_operations(merged_product_ptr[0], _encode_string(post_operations)) != 0:
                    raise CLibraryError()
            # Convert the merged C product into its Python representation.
            product = _import_product(merged_product_ptr[0])
        finally:
            _lib.harp_product_delete(merged_product_ptr[0])

        return product


def convert_unit(from_unit, to_unit, values):
    values = numpy.array(values, dtype=numpy.double)
    c_data = _ffi.cast("double *", values.ctypes.data)
    if _lib.harp_convert_unit(_encode_string(from_unit), _encode_string(to_unit), numpy.size(values), c_data) != 0:
        raise CLibraryError()
    return values


#
# Initialize the HARP Python interface.
#
_init()
