# TODO namespace?

.onLoad <- function(libname, pkgname)
{
    .Call("rharp_init")
}

.onUnload <- function(libpath)
{
    .Call("rharp_done")
}

import <- function(name, operations="", options="") {
    return(.Call("rharp_import_product", name, operations, options))
}

export <- function(product, name, file_format="netcdf") {
    return(.Call("rharp_export_product", product, name, file_format))
}
