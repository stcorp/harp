"""HARP Python interface

This module implements the HARP Python interface. The interface consists of a
minimal set of methods to import and export HARP products, and to ingest
non-HARP products.

Imported or ingested products are returned as pure Python objects, that can be
manipulated freely from within Python. It is also possible to build up a product
from scratch directly in Python. All data is represented as NumPy ndarrays.

HARP products are represented in Python by instances of the Product class. A
Product instance contains a Variable instance for each HARP variable. A Variable
instance contains both the data as well as the attributes (dimensions, unit,
valid range, description) of the corresponding HARP variable.

Variables can be accessed by name using either the attribute access '.' syntax,
or the item access '[]' syntax. For example:

    # Alternative ways to access the variable 'HCHO_column_number_density'.
    product.HCHO_column_number_density
    product["HCHO_column_number_density"]

    # Iterate over all variables in the product. For imported or ingested
    # products, the order of the variables is the same as the order in the
    # source product.
    for name in product:
        print product[name].unit

Product attributes can be accessed in the same way as variables, but are not
included when iterating over the variables in a product. For example:

    # Print product attributes.
    print product.source_product
    print product.history

The names of product attributes are reserved and cannot be used for variables.

A Product can be converted to an OrderedDict using the to_dict() module level
method. The OrderedDict representation provides direct access to the data
associated with each variable. All product attributes and all variable
attributes except the unit attribute are discarded as part of the conversion.
The unit attribute of a variable is represented by mapping the name of the
corresponding variable suffxed with '_unit' to a string containing the unit.

    # Accessing the variable 'HCHO_column_number_density'.
    product["HCHO_column_number_density"]

    # Accessing the unit attribute of the variable 'HCHO_column_number_density'.
    product["HCHO_column_number_density_unit"]

    # Iterate over all variables in the product. For imported or ingested
    # products, the order of the variables is the same as the order in the
    # source product.
    for name, value in product.iteritems():
        print name, value

The OrderedDict representation can be convenient when there is a need to
interface with existing code such as plotting libraries, or when the additional
information provided by the Product representation is not needed.

Note that only Product instances can be exported as a HARP product. The
OrderedDict representation does not contain enough information.

"""
import collections
import numpy
import platform
import StringIO

from _harpc import ffi as _ffi

__all__ = ["Error", "CLibraryError", "UnsupportedTypeError", "UnsupportedDimensionError", "Variable", "Product",
           "version", "ingest_product", "import_product", "export_product", "to_dict"]

class Error(Exception):
    """Exception base class for all HARP Python interface errors."""
    pass

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
            strerror = _ffi.string(_lib.harp_errno_to_string(errno))

        super(CLibraryError, self).__init__(errno, strerror)
        self.errno = errno
        self.strerror = strerror

    def __str__(self):
        return self.strerror

class Variable(object):
    """Python representation of a HARP variable.

    A variable consists of data (either a scalar of an N-dimensional array), a list
    of dimension types that describe the dimensions of the data, and a number of
    optional attributes: physical units, valid range (min, max), and a textual
    description.
    """
    def __init__(self, data, dimensions=[], unit=None, valid_min=None, valid_max=None, description=None):
        self.data = data
        self.dimensions = dimensions

        if unit:
            self.unit = unit
        if valid_min:
            self.valid_min = valid_min
        if valid_max:
            self.valid_max = valid_max
        if description:
            self.description = description

    def __repr__(self):
        if not self.dimensions:
            return "<Variable type=%s>" % _format_data_type(self.data)

        return "<Variable type=%s dimensions=%s>" % (_format_data_type(self.data),
                                                     _format_dimensions(self.dimensions, self.data))

    def __str__(self):
        stream = StringIO.StringIO()

        print >> stream, "type =", _format_data_type(self.data)

        if self.dimensions:
            print >> stream, "dimensions =", _format_dimensions(self.dimensions, self.data)

        unit = getattr(self, "unit", None)
        if unit:
            print >> stream, "unit =", repr(unit) if isinstance(unit, str) else "<invalid>"

        valid_min = getattr(self, "valid_min", None)
        if valid_min is not None:
            print >> stream, "valid_min =", repr(valid_min) if numpy.isscalar(valid_min) else "<invalid>"

        valid_max = getattr(self, "valid_max", None)
        if valid_max is not None:
            print >> stream, "valid_max =", repr(valid_max) if numpy.isscalar(valid_max) else "<invalid>"

        description = getattr(self, "description", None)
        if description:
            print >> stream, "description =", repr(description) if isinstance(description, str) else "<invalid>"

        if self.data is not None:
            if not isinstance(self.data, numpy.ndarray) and not numpy.isscalar(self.data):
                print >> stream, "data = <invalid>"
            elif numpy.isscalar(self.data):
                print >> stream, "data = %r" % self.data
            elif not self.dimensions and self.data.size == 1:
                print >> stream, "data = %r" % numpy.asscalar(self.data)
            elif self.data.size == 0:
                print >> stream, "data = <empty>"
            else:
                print >> stream, "data ="
                print >> stream, str(self.data)

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
        self._variable_dict = collections.OrderedDict()

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
        stream = StringIO.StringIO()

        # Attributes.
        source_product = getattr(self, "source_product", None)
        history = getattr(self, "history", None)

        if source_product:
            print >> stream, "source product =", \
                             repr(source_product) if isinstance(source_product, str) else "<invalid>"

        if history:
            print >> stream, "history =", repr(history) if isinstance(history, str) else "<invalid>"

        # Variables.
        if not self._variable_dict:
            return stream.getvalue()

        if history or source_product:
            stream.write("\n")

        for name, variable in self._variable_dict.iteritems():
            if not isinstance(variable, Variable):
                print >> stream, "<non-compliant variable %r>" % name
                continue

            if not isinstance(variable.data, numpy.ndarray) and not numpy.isscalar(variable.data):
                print >> stream, "<non-compliant variable %r>" % name
                continue

            if isinstance(variable.data, numpy.ndarray) and variable.data.size == 0:
                print >> stream, "<empty variable %r>" % name
                continue

            # Data type and variable name.
            stream.write(_format_data_type(variable.data) + " " + name)

            # Dimensions.
            if variable.dimensions:
                stream.write(" " + _format_dimensions(variable.dimensions, variable.data))

            # Unit.
            unit = getattr(variable, "unit", None)
            if unit:
                stream.write(" [" + (unit if isinstance(unit, str) else "<invalid>") + "]")

            stream.write("\n")

        return stream.getvalue()

def _get_c_library_filename():
    """Return the filename of the HARP shared library depending on the current
    platform.

    """
    from platform import system as _system

    return "harp.dll" if _system() == "Windows" else "harp.dylib" if _system() == "Darwin" else "libharp.so"

def _init():
    """Initialize the HARP Python interface."""
    global _lib, _py_dimension_type, _c_dimension_type, _py_data_type, _c_data_type_name

    # Initialize the HARP C library
    _lib = _ffi.dlopen(_get_c_library_filename())

    if _lib.harp_init() != 0:
        raise CLibraryError()

    # Initialize various look-up tables used thoughout the HARP Python interface (i.e. this module).
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

def _any(predicate, sequence):
    """Return True if the predicate evaluates to True for any element in the
    sequence, False otherwise.

    Attributes:
        predicate   --  Predicate to use; this should be a callable that takes a
                        single argument and returns a bool.
        sequence    --  Sequence to test.

    """
    return reduce(lambda x, y: x or predicate(y), sequence, False)

def _all(predicate, sequence):
    """Return True if the predicate evaluates to True for all elements in the
    sequence, False otherwise.

    Attributes:
        predicate   --  Predicate to use; this should be a callable that takes a
                        single argument and returns a bool.
        sequence    --  Sequence to test.

    """
    return reduce(lambda x, y: x and predicate(y), sequence, True)

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
    if isinstance(value, numpy.ndarray):
        # For NumPy ndarrays determine the smallest HARP C data type that can safely contain elements of the ndarray's
        # dtype.
        if numpy.issubdtype(value.dtype, numpy.object_):
            # NumPy object arrays are only used to contain variable length strings.
            if _all(lambda element: isinstance(element, str), value.flat):
                return _lib.harp_type_string

            raise UnsupportedTypeError("NumPy object array should only contain elements of type 'str'")

        if numpy.issubdtype(value.dtype, numpy.str_):
            return _lib.harp_type_string

        # Split tests for integer and floating point types, because numpy.can_cast() seems to accept casts from integer
        # to float in "safe" mode that cannot preserve values. In particular, it accepts casts from numpy.int64 to
        # numpy.float64.
        if numpy.issubdtype(value.dtype, numpy.integer):
            if numpy.can_cast(value.dtype, numpy.int8):
                return _lib.harp_type_int8
            if numpy.can_cast(value.dtype, numpy.int16):
                return _lib.harp_type_int16
            if numpy.can_cast(value.dtype, numpy.int32):
                return _lib.harp_type_int32

        if numpy.issubdtype(value.dtype, numpy.floating):
            if numpy.can_cast(value.dtype, numpy.float32):
                return _lib.harp_type_float
            if numpy.can_cast(value.dtype, numpy.float64):
                return _lib.harp_type_double

        raise UnsupportedTypeError("unsupported NumPy dtype '%s'" % value.dtype)

    if isinstance(value, numpy.generic):
        # Convert NumPy array scalars to the corresponding Python scalar.
        value = numpy.asscalar(value)

    if isinstance(value, str):
        return _lib.harp_type_string

    if isinstance(value, int) or isinstance(value, long):
        if value >= -2**7 and value <= 2**7 - 1:
            return _lib.harp_type_int8
        elif value >= -2**15 and value <= 2**15 - 1:
            return _lib.harp_type_int16
        elif value >= -2**31 and value <= 2**31 - 1:
            return _lib.harp_type_int32

        raise UnsupportedTypeError("integer value '%d' outside maximum supported range [%d, %d]" % (value, -2**31,
                                                                                                    2**31 - 1))
    if isinstance(value, float):
        # Use the floating point type with the highest precision, since the precision to exactly represent the specified
        # Python value is not trivial to determine.
        return _lib.harp_type_double

    raise UnsupportedTypeError("unsupported type %r" % value.__class__.__name__)

def _get_c_data_type_name(data_type):
    """Return the canonical name for the specified C data type code."""
    try:
        return _c_data_type_name[data_type]
    except KeyError:
        raise UnsupportedTypeError("unsupported C data type code '%d'" % data_type)

def _is_compatible_c_data_type(c_data_type_src, c_data_type_dst):
    """Returns True if the source C data type can be cast to the destination C data
    type while preserving values.

    """
    if c_data_type_dst == _lib.harp_type_int8:
        return c_data_type_src == _lib.harp_type_int8
    elif c_data_type_dst == _lib.harp_type_int16:
        return c_data_type_src in (_lib.harp_type_int8, _lib.harp_type_int16)
    elif c_data_type_dst == _lib.harp_type_int32:
        return c_data_type_src in (_lib.harp_type_int8, _lib.harp_type_int16, _lib.harp_type_int32)
    elif c_data_type_dst == _lib.harp_type_float:
        return c_data_type_src in (_lib.harp_type_int8, _lib.harp_type_int16, _lib.harp_type_float)
    elif c_data_type_dst == _lib.harp_type_double:
        return c_data_type_src in (_lib.harp_type_int8, _lib.harp_type_int16, _lib.harp_type_int32,
                                   _lib.harp_type_float, _lib.harp_type_double)
    elif c_data_type_dst == _lib.harp_type_string:
        return c_data_type_src == _lib.harp_type_string

    return False

def _as_py_scalar(value):
    """Return a Python scalar.

    If the input is a NumPy ndarray of a NumPy scalar, convert it to a Python
    scalar using numpy.asscalar(). Otherwise, the input is passed straight
    through.

    """
    if isinstance(value, numpy.ndarray) or isinstance(value, numpy.generic):
        return numpy.asscalar(value)

    return value

def _format_data_type(data):
    """Return the string representation of the C data type that would be used to
    store the specified data, or "<invalid>" if the specified data is of an
    unsupported type.

    """
    try:
        return _get_c_data_type_name(_get_c_data_type(data))
    except UnsupportedTypeError:
        return "<invalid>"

def _format_dimensions(dimensions, data):
    """Construct a formatted string from the specified dimensions and data that
    provides information about dimension types and lengths, or "<invalid>" if this
    information cannot be determined.

    """
    if not isinstance(data, numpy.ndarray) or data.ndim != len(dimensions):
        return "{<invalid>}"

    stream = StringIO.StringIO()

    stream.write("{")
    for i in range(data.ndim):
        if dimensions[i]:
            stream.write(dimensions[i] + "=")
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
            data[i] = _ffi.string(c_data.string_data[i])

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
        # Scalar.
        variable = Variable(numpy.asscalar(data))
    else:
        # N-dimensional array.
        data = data.reshape([c_variable.dimension[i] for i in range(num_dimensions)])
        dimensions = [_get_py_dimension_type(c_variable.dimension_type[i]) for i in range(num_dimensions)]
        variable = Variable(data, dimensions)

    # Import variable attributes.
    if c_variable.unit:
        variable.unit = _ffi.string(c_variable.unit)

    if c_variable.data_type != _lib.harp_type_string:
        variable.valid_min = _import_scalar(c_variable.data_type, c_variable.valid_min)
        variable.valid_max = _import_scalar(c_variable.data_type, c_variable.valid_max)

    if c_variable.description:
        variable.description = _ffi.string(c_variable.description)

    return variable

def _import_product(c_product):
    product = Product()

    # Import product attributes.
    if c_product.source_product:
        product.source_product = _ffi.string(c_product.source_product)

    if c_product.history:
        product.history = _ffi.string(c_product.history)

    # Import variables.
    for i in range(c_product.num_variables):
        c_variable_ptr = c_product.variable[i]
        variable = _import_variable(c_variable_ptr[0])
        setattr(product, _ffi.string(c_variable_ptr[0].name), variable)

    return product

def _export_scalar(c_data_type, c_data, data):
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

def _export_variable(name, variable, c_product):
    data = getattr(variable, "data", None)
    dimensions = getattr(variable, "dimensions", [])
    scalar = not dimensions

    if data is None:
        raise Error("no data or data is None")

    if not scalar and (not isinstance(data, numpy.ndarray) or data.ndim != len(dimensions)):
        raise Error("incorrect dimension information")

    if scalar and isinstance(data, numpy.ndarray):
        if data.size == 1:
            data = numpy.asscalar(data)
        else:
            raise Error("dimension information missing or incomplete")

    # Determine C dimension types and lengths.
    c_num_dimensions = len(dimensions)
    c_dimension_type = [_get_c_dimension_type(dimension) for dimension in dimensions]
    c_dimension = _ffi.NULL if scalar else data.shape

    # Determine C data type.
    c_data_type = _get_c_data_type(data)

    # Create C variable of the proper size.
    c_variable_ptr = _ffi.new("harp_variable **")
    if _lib.harp_variable_new(name, c_data_type, c_num_dimensions, c_dimension_type, c_dimension, c_variable_ptr) != 0:
        raise CLibraryError()

    # Add C variable to C product.
    if _lib.harp_product_add_variable(c_product, c_variable_ptr[0]) != 0:
        _lib.harp_variable_delete(c_variable_ptr[0])
        raise CLibraryError()

    # The C variable has been successfully added to the C product. Thus, the memory management of the C variable is
    # now tied to the life time of the C product and _lib.harp_variable_delete() should not be called. If an error
    # occurs, the memory occupied by the C variable will be freed when the C product is deleted.
    c_variable = c_variable_ptr[0]
    c_data = c_variable.data

    # Copy data into the C variable.
    if scalar:
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
        elif c_data_type == _lib.harp_type_string:
            if _lib.harp_variable_set_string_data_element(c_variable, 0, data) != 0:
                raise CLibraryError()
        else:
            raise UnsupportedTypeError("unsupported C data type code '%d'" % c_data_type)
    elif c_data_type == _lib.harp_type_string:
        for index, value in enumerate(data.flat):
            if _lib.harp_variable_set_string_data_element(c_variable, index, value) != 0:
                raise CLibraryError()
    else:
        # NB. The _ffi.buffer() method as well as the numpy.frombuffer() method provide a view on the C array;
        # neither method incurs a copy.
        c_data_buffer = _ffi.buffer(c_data.ptr, c_variable.num_elements * _lib.harp_get_size_for_type(c_data_type))
        c_data = numpy.reshape(numpy.frombuffer(c_data_buffer, dtype=_get_py_data_type(c_data_type)), data.shape)
        numpy.copyto(c_data, data, casting="safe")

    valid_min = getattr(variable, "valid_min", None)
    if valid_min is not None:
        try:
            valid_min = _as_py_scalar(valid_min)
        except ValueError:
            raise Error("valid_min attribute should be scalar")

        c_data_type_valid_min = _get_c_data_type(valid_min)
        if not _is_compatible_c_data_type(c_data_type_valid_min, c_data_type):
            raise Error("C data type '%s' of valid_min attribute is incompatible with C data type '%s' of data"
                        % (_get_c_data_type_name(c_data_type_valid_min), _get_c_data_type_name(c_data_type)))

        _export_scalar(c_data_type, c_variable_ptr[0].valid_min, valid_min)

    valid_max = getattr(variable, "valid_max", None)
    if valid_max is not None:
        try:
            valid_max = _as_py_scalar(valid_max)
        except ValueError:
            raise Error("valid_max attribute should be scalar")

        c_data_type_valid_max = _get_c_data_type(valid_max)
        if not _is_compatible_c_data_type(c_data_type_valid_max, c_data_type):
            raise Error("C data type '%s' of valid_max attribute is incompatible with C data type '%s' of data"
                        % (_get_c_data_type_name(c_data_type_valid_max), _get_c_data_type_name(c_data_type)))

        _export_scalar(c_data_type, c_variable_ptr[0].valid_max, valid_max)

    unit = getattr(variable, "unit", None)
    if unit and _lib.harp_variable_set_unit(c_variable_ptr[0], str(unit)) != 0:
        raise CLibraryError()

    description = getattr(variable, "description", None)
    if description and _lib.harp_variable_set_description(c_variable_ptr[0], str(description)) != 0:
        raise CLibraryError()

def _export_product(product, c_product):
    # Export product attributes.
    source_product = getattr(product, "source_product", None)
    if source_product and _lib.harp_product_set_source_product(c_product, str(source_product)) != 0:
        raise CLibraryError()

    history = getattr(product, "history", None)
    if history and _lib.harp_product_set_history(c_product, str(history)) != 0:
        raise CLibraryError()

    # Export variables.
    for name in product:
        try:
            _export_variable(name, product[name], c_product)
        except Error as _error:
            _error.args = (_error.args[0] + " (variable %r)" % name,) + _error.args[1:]
            raise

def version():
    """Return HARP C library version."""
    return _ffi.string(_lib.libharp_version)

def to_dict(product):
    """Convert a Product to an OrderedDict.

    Arguments:
    product     --  Product to convert.

    """
    if not isinstance(product, Product):
        raise Error("to_dict() argument must be Product, not %r" % product.__class__.__name__)

    dict_ = collections.OrderedDict()

    for name in product:
        variable = product[name]

        try:
            dict_[name] = variable.data
            if variable.unit:
                dict_[name + "_unit"] = variable.unit
        except AttributeError:
            pass

    return dict_

def import_product(filename):
    """Import a HARP compliant product.

    The file format (NetCDF/HDF4/HDF5) of the product will be auto-detected.

    Arguments:
    filename        --  Filename of the product to import.

    """
    c_product_ptr = _ffi.new("harp_product **")
    if _lib.harp_import(filename, c_product_ptr) != 0:
        raise CLibraryError()

    try:
        return _import_product(c_product_ptr[0])
    finally:
        _lib.harp_product_delete(c_product_ptr[0])

def ingest_product(filename, actions="", options=""):
    """Ingest a product of a type supported by HARP.

    Arguments:
    filename        --  Filename of the product to ingest.
    actions         --  Actions to apply as part of the ingestion; should be
                        specified as a semi-colon separated string of actions.
    options         --  Ingestion module specific options; should be specified as a
                        semi-colon separated string of key=value pairs.

    """
    c_product_ptr = _ffi.new("harp_product **")
    if _lib.harp_ingest(filename, actions, options, c_product_ptr) != 0:
        raise CLibraryError()

    try:
        return _import_product(c_product_ptr[0])
    finally:
        _lib.harp_product_delete(c_product_ptr[0])

def export_product(product, filename, file_format="netcdf"):
    """Export a HARP compliant product.

    Arguments:
    product     --  Product to export.
    filename    --  Filename of the exported product.
    file_format --  File format to use; one of 'netcdf', 'hdf4', or 'hdf5'.

    """
    if not isinstance(product, Product):
        raise Error("export_product() argument must be Product, not %r" % product.__class__.__name__)

    # Create C product.
    c_product_ptr = _ffi.new("harp_product **")
    if _lib.harp_product_new(c_product_ptr) != 0:
        raise CLibraryError()

    try:
        _export_product(product, c_product_ptr[0])

        # Call harp_export()
        if _lib.harp_export(filename, file_format, c_product_ptr[0]) != 0:
            raise CLibraryError()
    finally:
        _lib.harp_product_delete(c_product_ptr[0])

#
# Initialize the HARP Python interface.
#
_init()
