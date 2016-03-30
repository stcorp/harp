# ST_CODADEF_VERSION(definitions_dir, product_class)
# --------------------------------------------------
# Determine version number of codadef files
AC_DEFUN([ST_CODADEF_VERSION],
[if test -d $1/$2 ; then
files=`find $1/$2 $1/$2/types $1/$2/products -maxdepth 1 -name "*.xml"`
CODADEF_VERSION_$2=`(for file in $files ; do grep last-modified $file | head -1 ; done) | sed -e 's:^.*last-modified=\"\([[^"]]*\).*$:\1:' | sort -r | head -1 | sed 's:-::g'`
else
file=`ls $1/$2-*.codadef | sort -r | head -1`
if test -z "$file" ; then
  echo Cannot find .codadef file for $2 in $1
  exit 1
fi
CODADEF_VERSION_$2=`basename $file | sed -e "s:$2-\(.*\).codadef:\1:"`
fi
AC_SUBST(CODADEF_VERSION_$2)
])# ST_CODADEF_VERSION
