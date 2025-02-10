DSSP-server
===========

This is the source code for the dssp webserver application.

Building
--------

To build the code, you will have to install the following first:

* [Yarnpkg](https://yarnpkg.com/), a package manager for the Javascript and CSS files.
* [libmcfp](https://github.com/mhekkel/libmcfp), a library for parsing command line arguments
* [mrc](https://github.com/mhekkel/mrc), a resource compiler.
* [date](https://github.com/HowardHinnant/date), a library that implements date/time routines in C++.
* [libzeep](https://github.com/mhekkel/libzeep), a library for building web applications in C++
* [libcifpp](https://github.com/PDB-REDO/libcifpp), a library for reading and manipulating mmCIF files.
* [libgxrio](https://github.com/mhekkel/libgxrio), a library to read and write compressed files using C++ streams.
* [libpqxx](https://pqxx.org/development/libpqxx/), a library to work with postgresql databanks.

After that, check out this repository and the included sub repository `dssp` followed by the usual cmake commands:

```console
git clone https://github.com/PDB-REDO/dssp-server --recurse-submodules
cd dssp-server
cmake -S . -B build
cmake --build build
cmake --install build
```

Then create a databank for dssp and load the schema from `schema.sql`. E.g.:

```console
createuser dssp-admin -P
createdb dssp-db -O dssp-admin
psql dssp-db -f schema.sql
```

You will then need to copy the file `dsspd.conf.dist` to `dsspd.conf` and edit it.
Place the resulting file in `/etc`.

After that, you can launch dsspd:

```console
dsspd start
```

To run the server in the foreground, run `dsspd start --no-daemon`. Log files
are written to `/var/log/dsspd`.

