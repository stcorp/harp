# config params
site_lib = "/usr/local/lib/R/site-library/harp"
version = "0.1"
platform = "x86_64-pc-linux-gnu"
ostype= "unix"
Rversion = "3.3.3"
date = ""

# clear harp dir
val = unlink(site_lib, recursive=TRUE)
if(identical(val, 1))
    quit(status=1)

# create required dir tree
metadir = paste(site_lib, "Meta", sep="/")
htmldir = paste(site_lib, "html", sep="/")
helpdir = paste(site_lib, "help", sep="/")
libsdir = paste(site_lib, "libs", sep="/")
Rdir = paste(site_lib, "R", sep="/")

val = dir.create(site_lib)
if(identical(val, FALSE))
    quit(status=1)
val = dir.create(metadir)
if(identical(val, FALSE))
    quit(status=1)
val = dir.create(htmldir)
if(identical(val, FALSE))
    quit(status=1)
val = dir.create(helpdir)
if(identical(val, FALSE))
    quit(status=1)
val = dir.create(libsdir)
if(identical(val, FALSE))
    quit(status=1)
val = dir.create(Rdir)
if(identical(val, FALSE))
    quit(status=1)

# create DESCRIPTION
f = file(paste(site_lib, "DESCRIPTION", sep="/"))
writeLines(c("Package: harp", paste("Version:", version)), f)
close(f)

# create NAMESPACE
f = file(paste(site_lib, "NAMESPACE", sep="/"))
writeLines(c("useDynLib(harp, .registration = TRUE, .fixes = \"C_\")", "export(import, export, version)"), f)
close(f)

# copy R wrapper
val = file.copy("harp.R", paste(Rdir, "harp", sep="/"))
if(identical(val, FALSE))
    quit(status=1)

# copy shared lib
val = file.copy("harp.so", libsdir)
if(identical(val, FALSE))
    quit(status=1)

# create Meta/package.rds
q = list(
    DESCRIPTION=c(Package="harp", Version=version, Built=""),
    Built=list(R=Rversion, Platform=platform, OStype=ostype, Date=date)
)
saveRDS(q, paste(metadir, "/package.rds", sep=""))
