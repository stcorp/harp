# ST_CODADEF_VERSION(definitions_dir, product_class)
# --------------------------------------------------
# Determine version number of codadef files
AC_DEFUN([ST_CODADEF_VERSION],
[if test -d [$]srcdir/$1/$2 ; then
files=`find [$]srcdir/$1/$2 [$]srcdir/$1/$2/types [$]srcdir/$1/$2/products -maxdepth 1 -name "*.xml"`
CODADEF_VERSION_$2=`(for file in $files ; do grep last-modified $file | head -1 ; done) | sed -e 's:^.*last-modified=\"\([[^"]]*\).*$:\1:' | sort -r | head -1 | sed 's:-::g'`
else
file=`ls [$]srcdir/$1/$2-*.codadef | sort -r | head -1`
if test -z "$file" ; then
  echo Cannot find .codadef file for $2 in [$]srcdir/$1
  exit 1
fi
CODADEF_VERSION_$2=`basename $file | sed -e "s:$2-\(.*\).codadef:\1:"`
fi
AC_SUBST(CODADEF_VERSION_$2)
AC_SUBST(CODADEF_FILE_$2, $2-${CODADEF_VERSION_$2}.codadef)
AC_SUBST(CODADEF_RULE_$2,
['
$1/$2-${CODADEF_VERSION_$2}.codadef:
	test -d $1 || [$](mkinstalldirs) $1
	@[$](CODADEFSH) [$](srcdir)/$1/$2 $1
	@if test ! -f $1/$2-${CODADEF_VERSION_$2}.codadef ; then \
	  echo $1/$2-${CODADEF_VERSION_$2}.codadef not created. ; \
	  echo run "./config.status --recheck" to update the .codadef version reference. ; \
	  exit 1 ; \
	 fi
'])
AM_SUBST_NOTMAKE(CODADEF_RULE_$2)
])# ST_CODADEF_VERSION
