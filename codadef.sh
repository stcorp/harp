#!/bin/sh
# create .codadef file from a directory containg xml definition files
# and write them to an (optional) output directory. The default output
# directory is the current directory

if test $# -eq 0 -o $# -gt 2 ; then
  echo "Usage: $0 [--date] <input directory> [<output directory>]"
  exit 1
fi

dateonly=no
if test ${1} = "--date" ; then
  dateonly=yes
  shift
fi

inputdir=${1}
targetdir=`pwd`

if test $# -eq 2 ; then
  # make targetdir an absolute path (because it will be referenced after
  # a cd to the input directory
  firstchar=`echo ${2} | sed -e 's:^\(.\).*$:\1:'`
  if test "X${firstchar}" = "X/" ; then
    targetdir=${2}
  else
    targetdir=${targetdir}/${2}
  fi
fi

# check for existence of the input
if test ! -d ${inputdir} ; then
  echo "Input Directory ${inputdir} does not exist." 
  exit 1
fi

# check for existence of the output (or create it)
if test -e ${targetdir} ; then
  # something exists
  if test ! -d ${targetdir} ; then
    echo "Output location exists and is not a directory."
    exit 1
  fi
else
  # does not exist - create it
  mkdir -p ${targetdir}
  if test $? -ne 0 ; then
    echo "Failed to create output directory ${targetdir}."
    exit 1
  fi
fi

class=`grep "<cd:ProductClass" ${inputdir}/index.xml | sed -e 's:^.*name=\"\([^"]*\).*$:\1:'`

if test ${dateonly} = "no" ; then
  echo Determining last modification date for $class definitions
fi
files=`find ${inputdir} ${inputdir}/types ${inputdir}/products -maxdepth 1 -name "*.xml"`
date=`(for file in $files ; do grep last-modified $file | head -1 ; done) | sed -e 's:^.*last-modified=\"\([^"]*\).*$:\1:' | sort -r | head -1 | sed 's:-::g'`

if test ${dateonly} = "yes" ; then
  echo ${date}
  exit 0
fi

echo Creating $class-$date.codadef
rm -f ${class}-${date}.codadef
cd ${inputdir}
echo ${date} > VERSION
zip -q ${targetdir}/${class}-${date}.codadef VERSION *.xml types/*.xml products/*.xml
status=$?
rm -f VERSION

exit $status
