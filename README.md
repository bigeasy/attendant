# Attendant

A C module that launches and monitors a library server process. For use with a
dynamically loaded library module that proxies calls to an implementation that
runs as a separate process.

# Hacking

Hacking requires CMake.

```console
$ mkdir build && cd build && cmake ..
$ make
```

On Linux, install CMake using your package manager. On OS X, I prefer homebrew.

For Windows, I'm building using the Express Edition of Visual Studio 10. You'll
need to download and run the [CMake
Installer](http://www.cmake.org/cmake/resources/software.html). Then you'll want
to generate a Visual Studio project.

```console
C:\attendant> mkdir build
C:\attendant> cd build
C:\attendant\build> cmake -G "Visual Studio 10" ..
```

Now you can open the project in the `C:\attendant` directory and start hacking.
