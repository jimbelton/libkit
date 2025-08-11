libkit
======

This library contains useful standalone routines, supplying a basic
"kit" for developing C code.

The library contains tensor mathematical operations in kit-tensor.c 
that implement operations similar to those in PyTorch. For documentation
see: pytorch.org.

The build depends on the mak submodule which must be checked out alongside libkit.
This is best done using the kit module.

To build debian packages for the debian or Ubuntu Linux distribution you're
building libkit on, run:

```make package
```

This will create two debian packages in the output subdirectory. For example,
libkit_2.11-dev_amd64.deb and libkit-dev_2.11-dev_amd64.deb. The release number
(2.11 in the example) is taken from the debian/changelog file. The libkit-#.##
package is the run time, and is required both for development and to run programs
that use libkit. The libkit-dev-#.## package is only required when building
programs using libkit.

To install these packges, use the dpkg command:

```sudo dpkg -i output/libkit_2.11-dev_amd64.deb output/libkit-dev_2.11-dev_amd64.deb
```
