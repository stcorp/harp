import os
import subprocess
import time
import unittest

import numpy

import harp


class TestRBindings(unittest.TestCase):
    def setup(self):
        pass

    def teardown(self):
        os.system('rm -f unittest.nc export.nc script.R')

    def import_export(self, code):
        f = open("script.R", "w")
        f.write('library(harp)\n')
        f.write('p <- harp::import("unittest.nc")\n')
        f.write(code)
        f.write('harp::export(p, "export.nc")\n')
        f.close()

        p = subprocess.Popen(['Rscript', 'script.R'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out, err = p.communicate()
        self.assertEqual(p.returncode, 0)

        return out.splitlines(), err.splitlines()

    def testSimplestProduct(self):
        """Check simplest product"""

        # create empty product
        product = harp.Product()
        harp.export_product(product, "unittest.nc")

        # IMPORT
        out, err = self.import_export("""
            print(p$source_product)
        """)

        self.assertEqual(len(out), 2)
        self.assertEqual(out[0], '[1] "unittest.nc"')
        self.assertEqual(out[1], 'NULL') # TODO why does export do this

        # EXPORT
        with self.assertRaises(harp.NoDataError): # TODO why the exception..?
            harp.import_product("export.nc")

    def testSimplestVariable(self):
        """Check simplest variable"""

        product = harp.Product()
        product.temp = harp.Variable(numpy.array([7.7,8.7,9.7,10.7], dtype=numpy.float32), ["time"])
        harp.export_product(product, "unittest.nc")

        # IMPORT
        out, err = self.import_export("""
            print(p$temp$name)
            print(p$temp$dimension)
            print(p$temp$type)
            print(p$temp$data)
        """)

        self.assertEqual(len(out), 5)
        self.assertEqual(out[0], '[1] "temp"')
        self.assertEqual(out[1], '[1] "time"')
        self.assertEqual(out[2], '[1] "float"')
        self.assertEqual(out[3], '[1]  7.7  8.7  9.7 10.7')
        self.assertEqual(out[4], 'NULL') # TODO why does export do this

        # EXPORT
        product = harp.import_product("export.nc")
        var = product.temp
        self.assertEqual(var.dimension, ['time'])
        self.assertEqual(var.data.size, 4)
        self.assertLess(abs(var.data[0]-7.7), 0.001)
        self.assertLess(abs(var.data[1]-8.7), 0.001)
        self.assertLess(abs(var.data[2]-9.7), 0.001)
        self.assertLess(abs(var.data[3]-10.7), 0.001)

    def testStringVariable(self):
        """Check string variable"""

        product = harp.Product()
        product.strings = harp.Variable(numpy.array(("foo", "bar", "baz")), ["time"])
        harp.export_product(product, "unittest.nc")
        out, err = self.import_export("""
            print(p$strings$name)
            print(p$strings$dimension)
            print(p$strings$type)
            print(p$strings$data)
        """)

        self.assertEqual(len(out), 5)
        self.assertEqual(out[0], '[1] "strings"')
        self.assertEqual(out[1], '[1] "time"')
        self.assertEqual(out[2], '[1] "string"')
        self.assertEqual(out[3], '[1] "foo" "bar" "baz"')
        self.assertEqual(out[4], 'NULL') # TODO why does export do this

        # EXPORT
        product = harp.import_product("export.nc")
        var = product.strings
        self.assertEqual(var.dimension, ['time'])
        self.assertEqual(var.data.size, 3)
        self.assertEqual(var.data[0], 'foo')
        self.assertEqual(var.data[1], 'bar')
        self.assertEqual(var.data[2], 'baz')
