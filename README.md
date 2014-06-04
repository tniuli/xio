# PROXYIO

[![Build Status](https://api.travis-ci.org/pipul/xio.png?branch=master)](https://travis-ci.org/pipul/xio)

**proxyio** is a socket library that provides several common communication patterns. for more details about this project, see [Home](http://proxyio.org)

## Build

To build proxyio :

    $ sh autogen.sh
    $ ./configure
    $ make
	$ make check
    $ sudo make install

## Python binding

To build proxyio for python language

	$ ./configure --enable-python
	$ make
	$ make check
	$ sudo make install

## Lua binding

To build proxyio for lua language

	$ ./configure --enable-lua
	$ make
	$ make check
	$ sudo make install

## Golang binding

To build proxyio for golang language

Building the proxyio in normal way, and then

	$ cd binding/go
	$ go build
	$ go test -v ./...

## PHP binding

To build proxyio for php language

	$ cd binding/php
	$ phpize
	$ ./configure --enable-xio
	$ make
	$ sudo make install

copy the xio.ini config file into /etc/php.d/

## Ruby binding

To build proxyio for ruby language

	$ cd binding/ruby
	$ ruby extconf.rb
	$ make
	$ sudo make install

## License

Copyright 2014 Dong Fang <yp.fangdong@gmail.com>.

Licensed under the MIT/X11 License: http://opensource.org/licenses/MIT
