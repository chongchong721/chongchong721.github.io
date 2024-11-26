---
layout: post
title:  "How to build Mitsuba0.6 on Ubuntu Linux"
categories: setup
---

#### Building Mitsuba 0.6 from source on Ubuntu 22.04

0. This is not general instructions for building Mitsuba 0.6. These steps are needed for **me** to build Mitsuba on 2 laptops with Pop OS 22.04(which is essentially a more user friendly Ubuntu 22.04), and these two laptops have already installed a lot of other libraries, which may cause dependency conflict and other issues. I wrote this in case I replace my old laptop with a new one.

1. `git clone https://github.com/mitsuba-renderer/mitsuba.git `

   - Switch to branch scons-python3. If you're using Python2, you can stick to main branch

2. install C++ boost library (boost should be installed under `/usr/include` and `/usr/lib`) Make sure you don't have multiversion of boost, which can results in link failure(Took me 5 hours to figure out). if you installed boost manually then **do not** `apt install libboost-all-dev` in step 3

3. `sudo apt-get install build-essential scons git libpng12-dev libjpeg-dev libilmbase-dev libxerces-c-dev libboost-all-dev libopenexr-dev glew-utils libglewmx-dev libxxf86vm-dev libeigen3-dev libfftw3-dev`

   - Some libraries may be named differently in current version of Ubuntu. e.g. `libpng12-dev` is called `libpng-dev`
   - make sure you successfully installed all the packages

4. With GUI support, install Qt5 `sudo apt-get install qtbase5-dev libqt5opengl5-dev libqt5xmlpatterns5-dev`

5. On Linux, we need to manually set some Qt5 path

   - Follow [this issue](https://github.com/mitsuba-renderer/mitsuba/issues/32#issuecomment-334696914) ,

     - if you installed Qt5 through package manager, it should located like

        `QTINCLUDE      = ['/usr/include/x86_64-linux-gnu']`
       `QTDIR          = ['/usr/lib/x86_64-linux-gnu/qt5']`

       QTINCLUDE is the directory your Qt5 header files are located

       QTDIR is where your Qt5 library(.so) located

     - the third and fourth steps might not be necessary

6. You can now try to build Mitsuba by calling scons

7. If you meet errors like

   ```
   include/mitsuba/core/constants.h:58:32: error: unable to find numeric literal operator ‘operator""f’
    #define RCPOVERFLOW_FLT   0x1p-128f
                                   ^
   ```

​		Change the c++ flag in config.py from `-std=c++11` to `-std=gnu++11`

8. You might also encouter error saying 

   ```
   ‘_1’ was not declared in this scope
   ```

   This is because you're using a newer version of boost. You can include the header file

   `#include <boost/bind.hpp>`

9. `source setpath.sh`

#### Add python bindings

1. When installing boost, you need to specify `./bootstrap.sh --with-python=yourpythonexecutable`, this will let boost compile its Boost.python library
2. detect_python.py needs some modification.
   1. ~~Add our boost library path to it (I install boost at /usr/local, which the python script does not search through)~~
      - After testing, you should only install your boost under /usr/include and /usr/lib, or the built python library won't find where libboost_python is. I don't want to change the sourcecode anymore.
   2. Add your python version(if your python version >3.6), as they did not consider this future version when they wrote the script.
   3. Might also need to add your python version in Sconsript.configure
3. Scons use python script to run, so theoretically you can debug why it is not detecting your python.
4. `setpath.sh` will only export PYTHON library path in current shell session. If you want to set it globally, add `source yourMitsubaPath/setpath.sh  ` to your .zshrc/.bashrc. This path might be still not working in IDE, still trying to figure out how to set this in PyCharm