harpcheck
=========

Verify that a file complies with the HARP data format conventions and can be
read by other HARP command line tools.

::

  Usage:
      harpcheck <input product file> [input product file...]
          If the product is a HARP product then verify that the
          product is HARP compliant.
          Otherwise, try to import the product using an applicable
          ingestion module and test the ingestion for all possible
          ingestion options.

      harpcheck -h, --help
          Show help (this text).

      harpcheck -v, --version
          Print the version number of HARP and exit.
