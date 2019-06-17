import codecs
import os
import subprocess
import time
import unittest
import sys

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
        f.write('x <- harp::export(p, "export.nc")\n') # x captures NULL from stdout
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

        self.assertEqual(len(out), 1)
        self.assertEqual(out[0], b'[1] "unittest.nc"')

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

        self.assertEqual(len(out), 4)
        self.assertEqual(out[0], b'[1] "temp"')
        self.assertEqual(out[1], b'[1] "time"')
        self.assertEqual(out[2], b'[1] "float"')
        self.assertEqual(out[3], b'[1]  7.7  8.7  9.7 10.7')

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

        # IMPORT
        out, err = self.import_export("""
            print(p$strings$name)
            print(p$strings$dimension)
            print(p$strings$type)
            print(p$strings$data)
        """)

        self.assertEqual(len(out), 4)
        self.assertEqual(out[0], b'[1] "strings"')
        self.assertEqual(out[1], b'[1] "time"')
        self.assertEqual(out[2], b'[1] "string"')
        self.assertEqual(out[3], b'[1] "foo" "bar" "baz"')

        # EXPORT
        product = harp.import_product("export.nc")
        var = product.strings
        self.assertEqual(var.dimension, ['time'])
        self.assertEqual(var.data.size, 3)
        self.assertEqual(var.data[0], 'foo')
        self.assertEqual(var.data[1], 'bar')
        self.assertEqual(var.data[2], 'baz')

    def testUnicode(self):
        """Check unicode support"""

        if sys.hexversion < 0x03000000:
            raise unittest.SkipTest("no automatic encoding/decoding for python2")

        harp.set_encoding('utf-8')

        toshio, maeda = u'\u524d\u7530', u'\u4fca\u592b'

        product = harp.Product()
        product.strings = harp.Variable(numpy.array((toshio, maeda)), ["time"])
        harp.export_product(product, "unittest.nc")

        # IMPORT
        out, err = self.import_export("""
            print(p$strings$data)
        """)

        self.assertEqual(len(out), 1)
        self.assertEqual(out[0], codecs.encode('[1] "%s" "%s"' % (toshio, maeda)), 'utf-8')

        # EXPORT
        product = harp.import_product("export.nc")
        var = product.strings
        self.assertEqual(var.data.size, 2)
        self.assertEqual(var.data[0], toshio)
        self.assertEqual(var.data[1], maeda)
